#include "LightManager.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Graphics/CommandContext.h"
#include "Engine/Camera.h"
#include "Graphics/BufferManager.h"
#include "Math/VectorMath.h"
#include "Engine/EngineAdjust.h"
#include "Math/MathHelper.h"
#include "Engine/EngineProfiling.h"
#include "ConstantBuffer.h"
#include "Engine/RenderMaterial.h"
#include "PostProcessing/PostProcessing.h"

//shaders
#include "CompiledShaders/FillLightGrid_8_CS.h"
#include "CompiledShaders/FillLightGrid_16_CS.h"
#include "CompiledShaders/FillLightGrid_24_CS.h"
#include "CompiledShaders/FillLightGrid_32_CS.h"
#include "CompiledShaders/BuildClusterCS.h"
#include "CompiledShaders/MaskUnUsedClusterCS.h"
#include "CompiledShaders/ClusterLightCullCS.h"

using namespace GlareEngine::Math;
using namespace GlareEngine;


// Must keep in sync with HLSL
struct LightData
{
	float PositionWS[3];
	float RadiusSquare;

	float Color[3];
	uint32_t Type;

	float PositionVS[3];
	uint32_t ShadowConeIndex;

	float ConeDir[3];
	float ConeAngles[2];

	float ShadowTextureMatrix[16];
};

struct Cluster 
{
	Vector4 minPoint;
	Vector4 maxPoint;
};

struct ClusterCulling
{
	Vector3 TileSizes;
	float LightCount;
};

struct ClusterLightGrid
{
	uint32_t offset;
	float count;
};

__declspec(align(16)) struct ClusterBuildConstants
{
	Matrix4 View;
	Matrix4 InvProj;
	XMFLOAT3 tileSizes;
	float nearPlane;
	XMFLOAT2 perTileSize;
	float ScreenWidth;
	float ScreenHeight;
	float farPlane;
};


__declspec(align(16)) struct UnUsedClusterMaskConstant
{
	XMFLOAT2 cluserFactor;
	XMFLOAT2 perTileSize;
	XMFLOAT3 tileSizes;
	float ScreenWidth;
	float ScreenHeight;
	float farPlane;
	float nearPlane;
};

__declspec(align(16)) struct TileConstants
{
	uint32_t ViewportWidth;
	uint32_t ViewportHeight;
	float InvTileDim;
	float RcpZMagic;
	Vector3 EyePositionWS;
	Matrix4 InverseViewProj;
	Matrix4 InverseProjection;
	uint32_t TileCount;
};


enum { eMinLightGridDimension = 8 };

namespace GlareEngine
{
	namespace Lighting
	{
		//light Tile size
		IntVar LightGridDimension(32, eMinLightGridDimension, 32, 8);

		//light Cluster size
		XMFLOAT2 ClusterFactor;
		XMFLOAT2 ClusterTileSize = XMFLOAT2(0, 0);
		XMFLOAT3 ClusterTiles = XMFLOAT3(0, 0, 32);
		XMFLOAT2 PreClusterTiles= XMFLOAT2(0, 0);
		uint32_t LightClusterGridDimension = 64;
		uint32_t ClusterGroupTheardSize = 4;
		XMFLOAT3 ClusterDispathSize = XMFLOAT3(0, 0, 0);

		//Light RootSignature
		RootSignature m_FillLightRootSig;

		//Four tiled light types
		RenderMaterial m_FillLightGridCS_8(L"Fill Light Grid 8 CS");
		RenderMaterial m_FillLightGridCS_16(L"Fill Light Grid 16 CS");
		RenderMaterial m_FillLightGridCS_24(L"Fill Light Grid 24 CS");
		RenderMaterial m_FillLightGridCS_32(L"Fill Light Grid 36 CS");

		//Cluster Material
		RenderMaterial m_BuildClusterCS(L"Build Cluster CS");
		RenderMaterial m_MaskUnUsedClusterCS(L"Mask UnUsed Cluster");
		RenderMaterial m_ClusterLightCullCS(L"Cluster Light Cull");

		LightData m_LightData[MaxLights];
		StructuredBuffer m_LightBuffer;
		StructuredBuffer m_LightGrid;

		StructuredBuffer m_LightCluster;
		StructuredBuffer m_ClusterLightGrid;
		StructuredBuffer m_UnusedClusterMask;
		StructuredBuffer m_GlobalLightIndexList;
		StructuredBuffer m_GlobalIndexOffset;

		uint32_t m_FirstConeLight;
		uint32_t m_FirstConeShadowedLight;

		//shadow map size
		enum { eShadowDimension = 512 };

		ShadowBuffer m_LightShadowArray;
		ShadowBuffer m_LightShadowTempBuffer;
		Matrix4 m_LightShadowMatrix[MaxShadowedLights];

		bool FirstUpdateViewSpace = true;

		Camera ConeShadowCamera[MaxShadowedLights];

		uint32_t ClusterLightOffset[] = { 0,0,0,0 };

		bool bNeedRebuildCluster = true;
	}
}

void Lighting::InitializeResources(const Camera& camera)
{
	m_FillLightRootSig.Reset(3, 2);
	m_FillLightRootSig.InitStaticSampler(0, SamplerLinearClampDesc);
	m_FillLightRootSig.InitStaticSampler(1, SamplerLinearBorderDesc);
	m_FillLightRootSig[0].InitAsConstantBuffer(0);
	m_FillLightRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
	m_FillLightRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
	m_FillLightRootSig.Finalize(L"FillLightRS");

	InitComputeMaterial(m_FillLightRootSig, m_FillLightGridCS_8, g_pFillLightGrid_8_CS);
	InitComputeMaterial(m_FillLightRootSig, m_FillLightGridCS_16, g_pFillLightGrid_16_CS);
	InitComputeMaterial(m_FillLightRootSig, m_FillLightGridCS_24, g_pFillLightGrid_24_CS);
	InitComputeMaterial(m_FillLightRootSig, m_FillLightGridCS_32, g_pFillLightGrid_32_CS);
	InitComputeMaterial(m_FillLightRootSig, m_BuildClusterCS, g_pBuildClusterCS);
	InitComputeMaterial(m_FillLightRootSig, m_MaskUnUsedClusterCS, g_pMaskUnUsedClusterCS);
	InitComputeMaterial(m_FillLightRootSig, m_ClusterLightCullCS, g_pClusterLightCullCS);

	ClusterTileSize = XMFLOAT2(LightClusterGridDimension, LightClusterGridDimension);

	// Assumes max resolution of 3840x2160 for light grid
	uint32_t lightGridCells = Math::DivideByMultiple(3840, LightGridDimension) * Math::DivideByMultiple(2160, LightGridDimension);
	uint32_t lightGridSizeBytes = lightGridCells * (1 + MaxTileLights);
	m_LightGrid.Create(L"m_LightGrid", lightGridSizeBytes, sizeof(UINT));

	//Cluster
	ClusterFactor.x = (float)ClusterTiles.z / Math::Log2(camera.GetFarZ() / camera.GetNearZ());
	ClusterFactor.y = -((float)ClusterTiles.z * Math::Log2(camera.GetNearZ())) / Math::Log2(camera.GetFarZ() / camera.GetNearZ());

	m_LightShadowArray.Create(L"Light Shadow Array", eShadowDimension, eShadowDimension, DXGI_FORMAT_R32_FLOAT, MaxShadowedLights);
	m_LightShadowTempBuffer.Create(L"Light Shadow Temp Buffer", eShadowDimension, eShadowDimension, DXGI_FORMAT_R32_FLOAT);

	m_LightBuffer.Create(L"Light Buffer", MaxLights, sizeof(LightData));

	m_GlobalIndexOffset.Create(L"Global Index Offset", ArraySize(ClusterLightOffset), sizeof(uint32_t), ClusterLightOffset);
}

void Lighting::CreateRandomLights(const Vector3 minBound, const Vector3 maxBound,const Vector3 offset)
{
	Vector3 BoundSize = maxBound - minBound - offset * 2.0f;
	Vector3 BoundBias = minBound + offset;

	srand((unsigned)time(NULL));

	auto RandUINT = []() -> uint32_t
	{
		return rand(); // [0, RAND_MAX]
	};

	auto RandFloat = [RandUINT]() -> float
	{
		return RandUINT() * (1.0f / RAND_MAX);
	};

	auto RandVector = [RandFloat]() -> Vector3
	{
		return Vector3(RandFloat(), RandFloat(), RandFloat());
	};


	auto randGaussian = [RandFloat]() -> float
	{
		//box-muller
		static bool gaussianPair = true;
		static float y2;

		if (gaussianPair)
		{
			gaussianPair = false;

			float x1, x2, w;
			do
			{
				x1 = 2 * RandFloat() - 1;
				x2 = 2 * RandFloat() - 1;
				w = x1 * x1 + x2 * x2;
			} while (w >= 1);

			w = sqrtf(-2 * logf(w) / w);
			y2 = x2 * w;
			return x1 * w;
		}
		else
		{
			gaussianPair = true;
			return y2;
		}
	};
	auto randVecGaussian = [randGaussian]() -> Vector3
	{
		return Normalize(Vector3(randGaussian(), randGaussian(), randGaussian()));
	};

	int pointLightCount = (MaxLights - MaxShadowedLights) / 2;
	for (uint32_t lightIndex = 0; lightIndex < MaxLights; lightIndex++)
	{
		Vector3 position = RandVector() * BoundSize + BoundBias;
		float lightRadius = 50;

		Vector3 color = RandVector();
		float colorScale = RandFloat();
		color = color* colorScale;

		uint32_t type;

		if (lightIndex < pointLightCount)
			type = 0;//point light
		else if (lightIndex < pointLightCount * 2)
			type = 1;//cone light
		else
			type = 2;//shadowed cone light

		Vector3 coneDir = randVecGaussian();
		float coneInner = RandFloat() * 0.2f * MathHelper::Pi;
		float coneOuter = coneInner + RandFloat() * 0.3f * MathHelper::Pi;
		if (type == 0)
		{
			color = color * 2;
		}
		else if (type == 1)
		{
			// emphasize cone lights
			color = color * 2.0f;
		}
		else
		{
			lightRadius *= 2;
			color = color * 3.0f;
		}

		Matrix4 shadowTextureMatrix;
		if (type == 2)
		{
			static int shadowLightIndex = 0;
			ConeShadowCamera[shadowLightIndex].LookAt(position, position + coneDir, Vector3(0, 1, 0));
			ConeShadowCamera[shadowLightIndex].SetLens(coneOuter * 2, 1.0f, lightRadius * 0.01f, lightRadius * 1);
			ConeShadowCamera[shadowLightIndex].UpdateViewMatrix();
			XMFLOAT4X4 ViewProj;
			XMStoreFloat4x4(&ViewProj, XMMatrixTranspose(ConeShadowCamera[shadowLightIndex].GetViewProjection()));
			m_LightShadowMatrix[shadowLightIndex] = (Matrix4)ViewProj;
			m_LightData[lightIndex].ShadowConeIndex = shadowLightIndex;
			shadowTextureMatrix =  m_LightShadowMatrix[shadowLightIndex]* Matrix4(AffineTransform(Matrix3::MakeScale(0.5f, -0.5f, 1.0f), Vector3(0.5f, 0.5f, 0.0f)));
			shadowLightIndex++;
		}
		

		m_LightData[lightIndex].PositionWS[0] = position.GetX();
		m_LightData[lightIndex].PositionWS[1] = position.GetY();
		m_LightData[lightIndex].PositionWS[2] = position.GetZ();
		m_LightData[lightIndex].RadiusSquare = lightRadius * lightRadius;
		m_LightData[lightIndex].Color[0] = color.GetX();
		m_LightData[lightIndex].Color[1] = color.GetY();
		m_LightData[lightIndex].Color[2] = color.GetZ();
		m_LightData[lightIndex].Type = type;
		m_LightData[lightIndex].ConeDir[0] = coneDir.GetX();
		m_LightData[lightIndex].ConeDir[1] = coneDir.GetY();
		m_LightData[lightIndex].ConeDir[2] = coneDir.GetZ();
		m_LightData[lightIndex].ConeAngles[0] = 1.0f / (cosf(coneInner) - cosf(coneOuter));
		m_LightData[lightIndex].ConeAngles[1] = cosf(coneOuter);
		std::memcpy(m_LightData[lightIndex].ShadowTextureMatrix, &shadowTextureMatrix, sizeof(shadowTextureMatrix));
	}

	for (uint32_t n = 0; n < MaxLights; n++)
	{
		if (m_LightData[n].Type == 1)
		{
			m_FirstConeLight = n;
			break;
		}
	}
	for (uint32_t n = 0; n < MaxLights; n++)
	{
		if (m_LightData[n].Type == 2)
		{
			m_FirstConeShadowedLight = n;
			break;
		}
	}

	CommandContext::InitializeBuffer(m_LightBuffer, m_LightData, MaxLights * sizeof(LightData));
}

void Lighting::FillLightGrid(GraphicsContext& gfxContext, const Camera& camera)
{
	ScopedTimer _prof(L"Fill Light Grid", gfxContext);

	if (camera.GetViewChange()|| FirstUpdateViewSpace)
	{
		FirstUpdateViewSpace = false;
		for (size_t i = 0; i < MaxLights; i++)
		{
			Vector4 positionWS = Vector4(m_LightData[i].PositionWS[0], m_LightData[i].PositionWS[1], m_LightData[i].PositionWS[2], 1.0f);
			Vector4 positionVS = positionWS * (Matrix4)camera.GetView();
			m_LightData[i].PositionVS[0] = positionVS.GetX();
			m_LightData[i].PositionVS[1] = positionVS.GetY();
			m_LightData[i].PositionVS[2] = positionVS.GetZ();
		}
		CommandContext::InitializeBuffer(m_LightBuffer, m_LightData, MaxLights * sizeof(LightData));
	}

	ComputeContext& Context = gfxContext.GetComputeContext();

	Context.SetRootSignature(m_FillLightRootSig);

	switch ((int)LightGridDimension)
	{
	case  8: Context.SetPipelineState(m_FillLightGridCS_8.GetComputePSO()); break;
	case 16: Context.SetPipelineState(m_FillLightGridCS_16.GetComputePSO()); break;
	case 24: Context.SetPipelineState(m_FillLightGridCS_24.GetComputePSO()); break;
	case 32: Context.SetPipelineState(m_FillLightGridCS_32.GetComputePSO()); break;
	default: assert(false); break;
	}

	Context.TransitionResource(m_LightBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(m_LightGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Context.SetDynamicDescriptor(1, 0, m_LightBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 1, g_SceneDepthBuffer.GetDepthSRV());
	Context.SetDynamicDescriptor(2, 0, m_LightGrid.GetUAV());

	uint32_t tileCountX = (uint32_t)(Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), LightGridDimension));
	uint32_t tileCountY = (uint32_t)(Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), LightGridDimension));

	float FarClipDist = camera.GetFarZ();
	float NearClipDist = camera.GetNearZ();
	const float RcpZMagic = NearClipDist / (FarClipDist - NearClipDist);

	TileConstants csConstants;
	csConstants.ViewportWidth = g_SceneColorBuffer.GetWidth();
	csConstants.ViewportHeight = g_SceneColorBuffer.GetHeight();
	csConstants.InvTileDim = 1.0f / LightGridDimension;
	csConstants.RcpZMagic = RcpZMagic;
	csConstants.TileCount = tileCountX;
	csConstants.EyePositionWS = (Vector3)camera.GetPosition();

	XMMATRIX Proj = camera.GetProj();
	XMMATRIX ViewProj = camera.GetViewProjection();
	XMVECTOR  Determinant = XMMatrixDeterminant(Proj);
	XMMATRIX InvProj = XMMatrixInverse(&Determinant, Proj);
	Determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Determinant, ViewProj);
	csConstants.InverseProjection = (Matrix4)InvProj;
	csConstants.InverseViewProj = (Matrix4)InvViewProj;
	Context.SetDynamicConstantBufferView(0, sizeof(TileConstants), &csConstants);
	//Dispatch
	Context.Dispatch(tileCountX, tileCountY, 1);
	//Transition light grid Resource to pixel shader resource
	Context.TransitionResource(m_LightBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(m_LightGrid, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Lighting::BuildCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants)
{
	if (bNeedRebuildCluster)
	{
		ComputeContext& Context = gfxContext.GetComputeContext();

		ClusterTileSize.x = ceil(g_SceneColorBuffer.GetWidth() / ClusterTiles.x);
		ClusterTileSize.y = ceil(g_SceneColorBuffer.GetHeight() / ClusterTiles.y);

		ClusterBuildConstants csConstants;
		csConstants.View = (Matrix4)mainConstants.View;
		csConstants.InvProj = (Matrix4)mainConstants.InvProj;
		csConstants.tileSizes = ClusterTiles;
		csConstants.nearPlane = mainConstants.NearZ;
		csConstants.perTileSize = ClusterTileSize;
		csConstants.ScreenWidth = g_SceneColorBuffer.GetWidth();
		csConstants.ScreenHeight = g_SceneColorBuffer.GetHeight();
		csConstants.farPlane = mainConstants.FarZ;

		Context.TransitionResource(m_LightCluster, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		Context.SetRootSignature(*m_BuildClusterCS.GetRootSignature());
		Context.SetPipelineState(m_BuildClusterCS.GetComputePSO());
		Context.SetDynamicConstantBufferView(0, sizeof(ClusterBuildConstants), &csConstants);
		Context.SetDynamicDescriptor(2, 0, m_LightCluster.GetUAV());

		Context.Dispatch(ClusterDispathSize.x, ClusterDispathSize.y, ClusterDispathSize.z);

		bNeedRebuildCluster = false;
	}
}

void Lighting::MaskUnUsedCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants)
{
	ComputeContext& Context = gfxContext.GetComputeContext();

	UnUsedClusterMaskConstant csConstants;
	csConstants.cluserFactor = ClusterFactor;
	csConstants.farPlane = mainConstants.FarZ;
	csConstants.nearPlane = mainConstants.NearZ;
	csConstants.perTileSize = ClusterTileSize;
	csConstants.ScreenHeight= g_SceneColorBuffer.GetHeight();
	csConstants.ScreenWidth = g_SceneColorBuffer.GetWidth();
	csConstants.tileSizes = ClusterTiles;

	Context.TransitionResource(m_UnusedClusterMask, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.SetRootSignature(*m_MaskUnUsedClusterCS.GetRootSignature());
	Context.SetPipelineState(m_MaskUnUsedClusterCS.GetComputePSO());
	Context.SetDynamicConstantBufferView(0, sizeof(UnUsedClusterMaskConstant), &csConstants);
	Context.SetDynamicDescriptor(2, 0, m_UnusedClusterMask.GetUAV());
	Context.SetDynamicDescriptor(1, 0, ScreenProcessing::GetLinearDepthBuffer()->GetSRV());
	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}

void GlareEngine::Lighting::ClusterLightingCulling(GraphicsContext& gfxContext)
{
	ComputeContext& Context = gfxContext.GetComputeContext();

	Context.TransitionResource(m_UnusedClusterMask, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(m_LightBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(m_LightCluster, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(m_ClusterLightGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(m_GlobalLightIndexList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(m_GlobalIndexOffset, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	Context.SetRootSignature(*m_ClusterLightCullCS.GetRootSignature());
	Context.SetPipelineState(m_ClusterLightCullCS.GetComputePSO());

	ClusterCulling csConstants;
	csConstants.TileSizes = ClusterTiles;
	csConstants.LightCount = MaxLights;

	Context.SetDynamicConstantBufferView(0, sizeof(ClusterCulling), &csConstants);

	Context.SetDynamicDescriptor(1, 0, m_LightBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 1, m_LightCluster.GetSRV());
	Context.SetDynamicDescriptor(1, 2, m_UnusedClusterMask.GetSRV());

	Context.SetDynamicDescriptor(2, 0, m_ClusterLightGrid.GetUAV());
	Context.SetDynamicDescriptor(2, 1, m_GlobalLightIndexList.GetUAV());
	Context.SetDynamicDescriptor(2, 2, m_GlobalIndexOffset.GetUAV());
	Context.Dispatch(ClusterDispathSize.x, ClusterDispathSize.y, ClusterDispathSize.z);
}

void Lighting::Update(MainConstants& mainConstants)
{
	ClusterTiles.x = (uint32_t)(Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), LightClusterGridDimension));
	ClusterTiles.y = (uint32_t)(Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), LightClusterGridDimension));

	if (ClusterTiles.x != PreClusterTiles.x || ClusterTiles.y != PreClusterTiles.y)
	{
		ClusterDispathSize.x = (uint32_t)(Math::DivideByMultiple(ClusterTiles.x, ClusterGroupTheardSize));
		ClusterDispathSize.y = (uint32_t)(Math::DivideByMultiple(ClusterTiles.y, ClusterGroupTheardSize));
		ClusterDispathSize.z = (uint32_t)(Math::DivideByMultiple(ClusterTiles.z, ClusterGroupTheardSize));

		int ClusterSize = ClusterTiles.x * ClusterTiles.y * ClusterTiles.z;

		m_LightCluster.Create(L"Light Cluster", ClusterSize, sizeof(Cluster));

		m_ClusterLightGrid.Create(L"Cluster Light Grid", ClusterSize, sizeof(ClusterLightGrid));

		m_UnusedClusterMask.Create(L"Unused Cluster Mask", ClusterSize, sizeof(float));

		m_GlobalLightIndexList.Create(L"Global Light Index List", ClusterSize * MaxClusterLights, sizeof(uint32_t));

		bNeedRebuildCluster = true;
		PreClusterTiles.x = ClusterTiles.x;
		PreClusterTiles.y = ClusterTiles.y;

		mainConstants.gCluserFactor = ClusterFactor;
		mainConstants.gPerTileSize = ClusterTileSize;
		mainConstants.gTileSizes = ClusterTiles;
	}

}

void Lighting::Shutdown(void)
{
	m_LightBuffer.Destroy();
	m_LightGrid.Destroy();
	m_LightShadowArray.Destroy();
	m_LightShadowTempBuffer.Destroy();
	m_LightCluster.Destroy();
	m_ClusterLightGrid.Destroy();
	m_UnusedClusterMask.Destroy();
}
