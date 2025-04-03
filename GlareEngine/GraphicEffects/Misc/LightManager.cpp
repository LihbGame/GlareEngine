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
#include "InstanceModel/SimpleModelGenerator.h"
#include "InstanceModel/InstanceModel.h"
#include "EngineGUI.h"


//shaders
#include "CompiledShaders/FillLightGrid_8_CS.h"
#include "CompiledShaders/FillLightGrid_16_CS.h"
#include "CompiledShaders/FillLightGrid_24_CS.h"
#include "CompiledShaders/FillLightGrid_32_CS.h"
#include "CompiledShaders/BuildClusterCS.h"
#include "CompiledShaders/MaskUnUsedClusterCS.h"
#include "CompiledShaders/ClusterLightCullCS.h"

#include "CompiledShaders/AreaLightMeshPS.h"
#include "CompiledShaders/AreaLightMeshVS.h"
#include "CompiledShaders/WireframePS.h"

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
	uint32_t AreaLightIndex;

	float ShadowTextureMatrix[16];
};

struct RectAreaLightData
{
	XMFLOAT3 PositionWS[4];
	XMFLOAT3 PositionVS[4];
	XMFLOAT3 PositionCenter;
	XMFLOAT3 LightDir;
	XMFLOAT3 Color;
	float RadiusSquare;
};

struct LineAreaLightData
{


};

struct DiskAreaLightData
{


};

struct Cluster 
{
	Vector4 minPoint;
	Vector4 maxPoint;
};

__declspec(align(16))struct ClusterCulling
{
	XMFLOAT3 TileSizes;
	float LightCount;
};

struct ClusterLightGrid
{
	uint32_t offset;
	float count;
};

__declspec(align(16))struct ClusterBuildConstants
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

__declspec(align(16))struct ClusterLightOffset
{
	uint32_t LightOffset[4] = { 0,0,0,0 };
};

__declspec(align(16))struct UnUsedClusterMaskConstant
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

struct LightRenderConstants
{
	XMFLOAT4X4		mWorldTransform = MathHelper::Identity4x4();
	XMFLOAT4X4		mTexTransform = MathHelper::Identity4x4();
	XMFLOAT3        mLightColor;
	int				mMaterialIndex;
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
		RenderMaterial* m_FillLightGridCS_8 = nullptr;
		RenderMaterial* m_FillLightGridCS_16 = nullptr;
		RenderMaterial* m_FillLightGridCS_24 = nullptr;
		RenderMaterial* m_FillLightGridCS_32 = nullptr;

		//Cluster Material
		RenderMaterial* m_BuildClusterCS = nullptr;
		RenderMaterial* m_MaskUnUsedClusterCS = nullptr;
		RenderMaterial* m_ClusterLightCullCS = nullptr;

		//AreaLight Material
		RenderMaterial* AreaLightMaterial = nullptr;

		LightData m_LightData[MaxLights];
		RectAreaLightData m_RectAreaLightData[MaxAreaLights];

		StructuredBuffer m_LightBuffer;
		StructuredBuffer m_LightGrid;

		StructuredBuffer m_LightCluster;
		StructuredBuffer m_ClusterLightGrid;
		StructuredBuffer m_UnusedClusterMask;
		StructuredBuffer m_GlobalLightIndexList;
		StructuredBuffer m_GlobalIndexOffset;
		StructuredBuffer m_GlobalRectAreaLightData;

		uint32_t m_FirstConeLight;
		uint32_t m_FirstConeShadowedLight;

		//shadow map size
		enum { eShadowDimension = 512 };

		ShadowBuffer m_LightShadowArray;
		ShadowBuffer m_LightShadowTempBuffer;
		Matrix4 m_LightShadowMatrix[MaxShadowedLights];

		bool FirstUpdateViewSpace = false;

		Camera ConeShadowCamera[MaxShadowedLights];

		ClusterLightOffset mClusterLightOffset;

		bool bNeedRebuildCluster = true;

		bool bLightDataChanged = true;

		float m_LightRadius = 50;
		float m_QuadAreaLightSize = 20;
		float m_AreaLightIntensityScale = 5;

		float m_AreaLightSizeScale = 1;
		float m_PointLightSizeScale = 0.1;
		float m_ConeLightSizeScale = 0.1;

		unique_ptr<InstanceModel> mQuadAreaLightModel = nullptr;
		unique_ptr<InstanceModel> mQuadPointLightModel = nullptr;
		unique_ptr<InstanceModel> mQuadConeLightModel = nullptr;

		vector<LightRenderConstants> mAreaLightIRC;
		vector<LightRenderConstants> mPointLightIRC;
		vector<LightRenderConstants> mConeLightIRC;

		bool bEnableTiledBaseLight=true;
		bool bEnablePointLight = true;
		bool bEnableAreaLight = false;
		bool bEnableConeLight = true;
		bool bEnableDirectionalLight = true;
		bool bEnableAreaLightTwoSide = true;

		uint32_t AreaLightLTC1SRVIndex;
		uint32_t AreaLightLTC2SRVIndex;
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

	//init shader
	InitMaterial();

	ClusterTileSize = XMFLOAT2(LightClusterGridDimension, LightClusterGridDimension);

	// Assumes max resolution of 3840x2160 for light grid
	uint32_t lightGridCells = Math::DivideByMultiple(3840, LightGridDimension) * Math::DivideByMultiple(2160, LightGridDimension);
	uint32_t lightGridSizeBytes = lightGridCells * (1 + MaxTileLights);
	m_LightGrid.Create(L"m_LightGrid", lightGridSizeBytes, sizeof(UINT));

	//Cluster
	ClusterFactor.x = (float)ClusterTiles.z / log2(camera.GetFarZ() / camera.GetNearZ());
	ClusterFactor.y = -((float)ClusterTiles.z * log2(camera.GetNearZ())) / log2(camera.GetFarZ() / camera.GetNearZ());

	m_LightShadowArray.Create(L"Light Shadow Array", eShadowDimension, eShadowDimension, DXGI_FORMAT_R32_FLOAT, MaxShadowedLights);
	m_LightShadowTempBuffer.Create(L"Light Shadow Temp Buffer", eShadowDimension, eShadowDimension, DXGI_FORMAT_R32_FLOAT);

	m_LightBuffer.Create(L"Light Buffer", MaxLights, sizeof(LightData));

	m_GlobalIndexOffset.Create(L"Global Index Offset", sizeof(ClusterLightOffset), sizeof(uint32_t), mClusterLightOffset.LightOffset);

	m_GlobalRectAreaLightData.Create(L"Global Rect Area Light Data", MaxAreaLights, sizeof(RectAreaLightData));
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

	int pointLightCount = (MaxLights - MaxShadowedLights- MaxAreaLights) / 2;

	for (uint32_t lightIndex = 0; lightIndex < MaxLights; lightIndex++)
	{
		Vector3 position = RandVector() * BoundSize + BoundBias;
		float lightRadius = m_LightRadius;

		Vector3 color = RandVector();
		//float colorScale = RandFloat();
		color = Normalize(color);

		LightType type;

		if (lightIndex < pointLightCount)
			type = LightType::PointLight;
		else if (lightIndex < pointLightCount * 2)
			type = LightType::ConeLight;
		else if (lightIndex < (pointLightCount * 2 + MaxShadowedLights))
			type = LightType::ShadowedConeLight;
		else
			type = LightType::AreaLight;

		Vector3 LightDir = randVecGaussian();
		float coneInner = RandFloat() * 0.2f * MathHelper::Pi;
		float coneOuter = coneInner + RandFloat() * 0.3f * MathHelper::Pi;

		if (type == LightType::PointLight|| type == LightType::AreaLight)
		{
			color = color * 2;
		}
		else
		{
			// emphasize cone lights
			lightRadius *= 2;
			color = color * coneInner;
		}

		Matrix4 shadowTextureMatrix;
		if (type == LightType::ShadowedConeLight)
		{
			static int shadowLightIndex = 0;
			ConeShadowCamera[shadowLightIndex].LookAt(position, position + LightDir, Vector3(0, 1, 0));
			ConeShadowCamera[shadowLightIndex].SetLens(coneOuter * 2, 1.0f, lightRadius * 0.01f, lightRadius * 1);
			ConeShadowCamera[shadowLightIndex].UpdateViewMatrix();
			XMFLOAT4X4 ViewProj;
			XMStoreFloat4x4(&ViewProj, XMMatrixTranspose(ConeShadowCamera[shadowLightIndex].GetViewProjection()));
			m_LightShadowMatrix[shadowLightIndex] = (Matrix4)ViewProj;
			m_LightData[lightIndex].ShadowConeIndex = shadowLightIndex;
			shadowTextureMatrix = m_LightShadowMatrix[shadowLightIndex] * Matrix4(AffineTransform(Matrix3::MakeScale(0.5f, -0.5f, 1.0f), Vector3(0.5f, 0.5f, 0.0f)));
			shadowLightIndex++;
		}
		else if (type == LightType::AreaLight)
		{
			static int AreaLightIndex = 0;
			lightRadius = m_QuadAreaLightSize*100.0f;

			m_RectAreaLightData[AreaLightIndex].PositionCenter = XMFLOAT3(position / 1.5f);
			m_RectAreaLightData[AreaLightIndex].LightDir = XMFLOAT3(LightDir);
			m_RectAreaLightData[AreaLightIndex].RadiusSquare = lightRadius * lightRadius;
			m_RectAreaLightData[AreaLightIndex].Color = XMFLOAT3(color);
			position=m_RectAreaLightData[AreaLightIndex].PositionCenter;
			m_LightData[lightIndex].AreaLightIndex=AreaLightIndex;
			AreaLightIndex++;
		}
		
		m_LightData[lightIndex].PositionWS[0] = position.GetX();
		m_LightData[lightIndex].PositionWS[1] = position.GetY();
		m_LightData[lightIndex].PositionWS[2] = position.GetZ();
		m_LightData[lightIndex].RadiusSquare = lightRadius * lightRadius;
		m_LightData[lightIndex].Color[0] = color.GetX();
		m_LightData[lightIndex].Color[1] = color.GetY();
		m_LightData[lightIndex].Color[2] = color.GetZ();
		m_LightData[lightIndex].Type = (uint32_t)type;
		m_LightData[lightIndex].ConeDir[0] = LightDir.GetX();
		m_LightData[lightIndex].ConeDir[1] = LightDir.GetY();
		m_LightData[lightIndex].ConeDir[2] = LightDir.GetZ();
		m_LightData[lightIndex].ConeAngles[0] = 1.0f / (cosf(coneInner) - cosf(coneOuter));
		m_LightData[lightIndex].ConeAngles[1] = cosf(coneOuter);
		std::memcpy(m_LightData[lightIndex].ShadowTextureMatrix, &shadowTextureMatrix, sizeof(shadowTextureMatrix));
	}
	
	FirstUpdateViewSpace = true;

	CommandContext::InitializeBuffer(m_LightBuffer, m_LightData, MaxLights * sizeof(LightData));
	CommandContext::InitializeBuffer(m_GlobalRectAreaLightData, m_RectAreaLightData, MaxAreaLights * sizeof(RectAreaLightData));
	CreateLightRenderData();
}


void Lighting::CreateLightRenderData()
{
	GraphicsContext& Context = GraphicsContext::Begin(L"Init Light Render data");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;

	// Area Light LTC
	D3D12_CPU_DESCRIPTOR_HANDLE AreaLightLTC1Srv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ID3D12Resource* AreaLightLTC1Resource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"Lighting/ltc_1", false)->Resource.Get();
	srvDesc.Format = AreaLightLTC1Resource->GetDesc().Format;
	g_Device->CreateShaderResourceView(AreaLightLTC1Resource, &srvDesc, AreaLightLTC1Srv);
	AreaLightLTC1SRVIndex = AddToGlobalCubeSRVDescriptor(AreaLightLTC1Srv);

	D3D12_CPU_DESCRIPTOR_HANDLE AreaLightLTC2Srv = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ID3D12Resource* AreaLightLTC2Resource = TextureManager::GetInstance(Context.GetCommandList())->GetTexture(L"Lighting/ltc_2", false)->Resource.Get();
	srvDesc.Format = AreaLightLTC2Resource->GetDesc().Format;
	g_Device->CreateShaderResourceView(AreaLightLTC2Resource, &srvDesc, AreaLightLTC2Srv);
	AreaLightLTC2SRVIndex = AddToGlobalCubeSRVDescriptor(AreaLightLTC2Srv);
	
	//area light mesh
	{
		ModelRenderData* ModelData = SimpleModelGenerator::GetInstance(Context.GetCommandList())->CreateGridRanderData("AreaLight", m_QuadAreaLightSize, m_QuadAreaLightSize, 2, 2);
		InstanceRenderData InstanceData;
		InstanceData.mModelData = ModelData;
		InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
		for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
		{
			for (int LightIndex = 0; LightIndex < MaxAreaLights; LightIndex++)
			{
				LightRenderConstants IRC;
				Vector3 rotAxis = Vector3(XMVector3Cross(Vector3(0, 1, 0), Vector3(m_RectAreaLightData[LightIndex].LightDir)));
				float rotAngle = std::acosf(Vector3(XMVector3Dot(Vector3(0, 1, 0), Vector3(m_RectAreaLightData[LightIndex].LightDir))).GetX());
				Matrix3 RotMatrix = Quaternion(rotAxis, rotAngle);
				Matrix4 LightTransform = Matrix4(Vector4(RotMatrix.GetX()), Vector4(RotMatrix.GetY()), Vector4(RotMatrix.GetZ()), Vector4(0, 0, 0, 1));
				IRC.mLightColor = XMFLOAT3(m_RectAreaLightData[LightIndex].Color);
				XMFLOAT3 position = XMFLOAT3(m_RectAreaLightData[LightIndex].PositionCenter);
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMATRIX(LightTransform) * XMMatrixScaling(m_AreaLightSizeScale, m_AreaLightSizeScale, m_AreaLightSizeScale) * XMMatrixTranslation(position.x, position.y, position.z)));
				mAreaLightIRC.push_back(IRC);
				
				MeshData meshData=ModelData->mSubModels[SubMeshIndex].mMeshData.GetMeshData();
				m_RectAreaLightData[LightIndex].PositionWS[0]=XMFLOAT3(Vector3(meshData.Vertices[0].Position)*RotMatrix+m_RectAreaLightData[LightIndex].PositionCenter);
				m_RectAreaLightData[LightIndex].PositionWS[1]=XMFLOAT3(Vector3(meshData.Vertices[2].Position)*RotMatrix+m_RectAreaLightData[LightIndex].PositionCenter);
				m_RectAreaLightData[LightIndex].PositionWS[2]=XMFLOAT3(Vector3(meshData.Vertices[3].Position)*RotMatrix+m_RectAreaLightData[LightIndex].PositionCenter);
				m_RectAreaLightData[LightIndex].PositionWS[3]=XMFLOAT3(Vector3(meshData.Vertices[1].Position)*RotMatrix+m_RectAreaLightData[LightIndex].PositionCenter);
			}
			InstanceConstants IC;
			IC.mDataPtr = mAreaLightIRC.data();
			IC.mDataNum = mAreaLightIRC.size();
			IC.mDataSize = sizeof(LightRenderConstants);
			InstanceData.mInstanceConstants[SubMeshIndex] = IC;
		}
		mQuadAreaLightModel = make_unique<InstanceModel>(StringToWString("Area Light"), InstanceData);
		mQuadAreaLightModel->SetShadowFlag(false);
	}

	//Point light mesh
	{
		const ModelRenderData* ModelData = SimpleModelGenerator::GetInstance(Context.GetCommandList())->CreateSimpleModelRanderData("PointLight", SimpleModelType::Sphere);
		InstanceRenderData InstanceData;
		InstanceData.mModelData = ModelData;
		InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
		int pointLightCount = (MaxLights - MaxShadowedLights - MaxAreaLights) / 2;
		for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
		{
			for (int LightIndex = 0; LightIndex < pointLightCount; ++LightIndex)
			{
				LightRenderConstants IRC;
				IRC.mLightColor = XMFLOAT3(m_LightData[LightIndex].Color);
				XMFLOAT3 position = XMFLOAT3(m_LightData[LightIndex].PositionWS);
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMatrixScaling(m_PointLightSizeScale, m_PointLightSizeScale, m_PointLightSizeScale) * XMMatrixTranslation(position.x, position.y, position.z)));
				mPointLightIRC.push_back(IRC);
			}
			InstanceConstants IC;
			IC.mDataPtr = mPointLightIRC.data();
			IC.mDataNum = mPointLightIRC.size();
			IC.mDataSize = sizeof(LightRenderConstants);
			InstanceData.mInstanceConstants[SubMeshIndex] = IC;
		}
		mQuadPointLightModel = make_unique<InstanceModel>(StringToWString("Point Light"), InstanceData);
		mQuadPointLightModel->SetShadowFlag(false);
	}

	//Cone light mesh
	{
		const ModelRenderData* ModelData = SimpleModelGenerator::GetInstance(Context.GetCommandList())->CreateSimpleModelRanderData("ConeLight", SimpleModelType::Cone);
		InstanceRenderData InstanceData;
		InstanceData.mModelData = ModelData;
		InstanceData.mInstanceConstants.resize(ModelData->mSubModels.size());
		int pointLightBegine = (MaxLights - MaxShadowedLights - MaxAreaLights) / 2;
		int pointLightEnd = pointLightBegine * 2 + MaxShadowedLights;

		for (int SubMeshIndex = 0; SubMeshIndex < ModelData->mSubModels.size(); SubMeshIndex++)
		{
			for (int LightIndex = pointLightBegine; LightIndex < pointLightEnd; ++LightIndex)
			{
				LightRenderConstants IRC;
				IRC.mLightColor = XMFLOAT3(m_LightData[LightIndex].Color);
				XMFLOAT3 position = XMFLOAT3(m_LightData[LightIndex].PositionWS);
				Vector3 ConeDir = Vector3(m_LightData[LightIndex].ConeDir[0], m_LightData[LightIndex].ConeDir[1], m_LightData[LightIndex].ConeDir[2]);
				Vector3 rotAxis = Vector3(XMVector3Cross(Vector3(0, 1, 0), ConeDir));
				float rotAngle = std::acosf(Vector3(XMVector3Dot(Vector3(0, 1, 0), ConeDir)).GetX());
				Matrix3 RotMatrix = Quaternion(rotAxis, rotAngle);
				Matrix4 LightTransform = Matrix4(Vector4(RotMatrix.GetX()), Vector4(RotMatrix.GetY()), Vector4(RotMatrix.GetZ()), Vector4(0, 0, 0, 1));
				XMStoreFloat4x4(&IRC.mWorldTransform, XMMatrixTranspose(XMMATRIX(LightTransform) * XMMatrixScaling(m_ConeLightSizeScale, m_ConeLightSizeScale, m_ConeLightSizeScale) * XMMatrixTranslation(position.x, position.y, position.z)));
				mConeLightIRC.push_back(IRC);
			}
			InstanceConstants IC;
			IC.mDataPtr = mConeLightIRC.data();
			IC.mDataNum = mConeLightIRC.size();
			IC.mDataSize = sizeof(LightRenderConstants);
			InstanceData.mInstanceConstants[SubMeshIndex] = IC;
		}
		mQuadConeLightModel = make_unique<InstanceModel>(StringToWString("Cone light"), InstanceData);
		mQuadConeLightModel->SetShadowFlag(false);
	}
	Context.Finish();
}


void Lighting::FillLightGrid(GraphicsContext& gfxContext, const Camera& camera)
{
	FillLightGrid_Internal(gfxContext, camera, g_SceneColorBuffer, g_SceneDepthBuffer);
}

void Lighting::FillLightGrid_Internal(GraphicsContext& gfxContext, const Camera& camera, 
	ColorBuffer& SceneColor, DepthBuffer& SceneDepth)
{
	ScopedTimer _prof(L"Fill Light Grid", gfxContext);

	ComputeContext& Context = gfxContext.GetComputeContext();

	Context.SetRootSignature(m_FillLightRootSig);

	switch ((int)LightGridDimension)
	{
	case  8: Context.SetPipelineState(m_FillLightGridCS_8->GetComputePSO()); break;
	case 16: Context.SetPipelineState(m_FillLightGridCS_16->GetComputePSO()); break;
	case 24: Context.SetPipelineState(m_FillLightGridCS_24->GetComputePSO()); break;
	case 32: Context.SetPipelineState(m_FillLightGridCS_32->GetComputePSO()); break;
	default: assert(false); break;
	}

	Context.TransitionResource(SceneDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetDynamicDescriptor(1, 0, m_LightBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 1, SceneDepth.GetDepthSRV());
	Context.SetDynamicDescriptor(2, 0, m_LightGrid.GetUAV());

	uint32_t tileCountX = (uint32_t)(Math::DivideByMultiple(SceneColor.GetWidth(), LightGridDimension));
	uint32_t tileCountY = (uint32_t)(Math::DivideByMultiple(SceneColor.GetHeight(), LightGridDimension));

	float FarClipDist = camera.GetFarZ();
	float NearClipDist = camera.GetNearZ();
	const float RcpZMagic = NearClipDist / (FarClipDist - NearClipDist);

	TileConstants csConstants;
	csConstants.ViewportWidth = SceneColor.GetWidth();
	csConstants.ViewportHeight = SceneColor.GetHeight();
	csConstants.InvTileDim = 1.0f / LightGridDimension;
	csConstants.RcpZMagic = RcpZMagic;
	csConstants.TileCount = tileCountX;
	csConstants.EyePositionWS = (Vector3)camera.GetPosition();

	XMMATRIX Proj = camera.GetProj();
	XMMATRIX ViewProj = camera.GetViewProjNoTranspose();
	XMVECTOR  Determinant = XMMatrixDeterminant(Proj);
	XMMATRIX InvProj = XMMatrixInverse(&Determinant, Proj);
	Determinant = XMMatrixDeterminant(ViewProj);
	XMMATRIX InvViewProj = XMMatrixInverse(&Determinant, ViewProj);
	csConstants.InverseProjection = (Matrix4)InvProj;
	csConstants.InverseViewProj = (Matrix4)InvViewProj;
	Context.SetDynamicConstantBufferView(0, sizeof(TileConstants), &csConstants);
	//Dispatch
	Context.Dispatch(tileCountX, tileCountY, 1);
}


void Lighting::BuildCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants)
{
	if (bNeedRebuildCluster)
	{
		ScopedTimer _prof(L"Build Cluster", gfxContext);
		ComputeContext& Context = gfxContext.GetComputeContext();

		ClusterBuildConstants csConstants;
		csConstants.View = (Matrix4)mainConstants.View;
		csConstants.InvProj = (Matrix4)mainConstants.InvProj;
		csConstants.tileSizes = ClusterTiles;
		csConstants.nearPlane = mainConstants.NearZ;
		csConstants.perTileSize = ClusterTileSize;
		csConstants.ScreenWidth = g_SceneColorBuffer.GetWidth();
		csConstants.ScreenHeight = g_SceneColorBuffer.GetHeight();
		csConstants.farPlane = mainConstants.FarZ;

		Context.SetRootSignature(*m_BuildClusterCS->GetRootSignature());
		Context.SetPipelineState(m_BuildClusterCS->GetComputePSO());
		Context.SetDynamicConstantBufferView(0, sizeof(ClusterBuildConstants), &csConstants);
		Context.SetDynamicDescriptor(2, 0, m_LightCluster.GetUAV());

		Context.Dispatch(ClusterDispathSize.x, ClusterDispathSize.y, ClusterDispathSize.z);

		bNeedRebuildCluster = false;
	}
}

void Lighting::MaskUnUsedCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants)
{
	ScopedTimer _prof(L"Mask UnUsed Cluster", gfxContext);
	ComputeContext& Context = gfxContext.GetComputeContext();

	UnUsedClusterMaskConstant csConstants;
	csConstants.cluserFactor = ClusterFactor;
	csConstants.farPlane = mainConstants.FarZ;
	csConstants.nearPlane = mainConstants.NearZ;
	csConstants.perTileSize = ClusterTileSize;
	csConstants.ScreenHeight= g_SceneColorBuffer.GetHeight();
	csConstants.ScreenWidth = g_SceneColorBuffer.GetWidth();
	csConstants.tileSizes = ClusterTiles;

	Context.SetRootSignature(*m_MaskUnUsedClusterCS->GetRootSignature());
	Context.SetPipelineState(m_MaskUnUsedClusterCS->GetComputePSO());
	Context.SetDynamicConstantBufferView(0, sizeof(UnUsedClusterMaskConstant), &csConstants);
	Context.SetDynamicDescriptor(2, 0, m_UnusedClusterMask.GetUAV());
	Context.SetDynamicDescriptor(1, 0, ScreenProcessing::GetLinearDepthBuffer()->GetSRV());
	Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
}

void Lighting::ClusterLightingCulling(GraphicsContext& gfxContext)
{
	ScopedTimer _prof(L"Cluster Light Culling", gfxContext);
	ComputeContext& Context = gfxContext.GetComputeContext();

	Context.SetRootSignature(*m_ClusterLightCullCS->GetRootSignature());
	Context.SetPipelineState(m_ClusterLightCullCS->GetComputePSO());

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

void Lighting::Update(MainConstants& mainConstants,Camera& camera)
{
	UpdateViewSpacePosition(camera);

	ClusterTiles.x = (uint32_t)(Math::DivideByMultiple(g_SceneColorBuffer.GetWidth(), LightClusterGridDimension));
	ClusterTiles.y = (uint32_t)(Math::DivideByMultiple(g_SceneColorBuffer.GetHeight(), LightClusterGridDimension));

	CommandContext::InitializeBuffer(m_GlobalIndexOffset, mClusterLightOffset.LightOffset, sizeof(ClusterLightOffset));

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

void Lighting::UpdateViewSpacePosition(Camera& camera)
{
	if (camera.GetViewChange() || FirstUpdateViewSpace || bLightDataChanged)
	{
		FirstUpdateViewSpace = false;
		bLightDataChanged = false;
		for (size_t i = 0; i < MaxLights; i++)
		{
			Vector4 positionWS = Vector4(m_LightData[i].PositionWS[0], m_LightData[i].PositionWS[1], m_LightData[i].PositionWS[2], 1.0f);
			Vector4 positionVS = positionWS * (Matrix4)camera.GetView();
			m_LightData[i].PositionVS[0] = positionVS.GetX();
			m_LightData[i].PositionVS[1] = positionVS.GetY();
			m_LightData[i].PositionVS[2] = positionVS.GetZ();
		}
		//update Area Lights
		for (size_t i = 0; i < MaxAreaLights; i++)
		{
			for (size_t numVert = 0; numVert < 4; numVert++)
			{
				Vector4 positionWS = Vector4(m_RectAreaLightData[i].PositionWS[numVert], 1.0f);
				Vector4 positionVS = positionWS * (Matrix4)camera.GetView();
				m_RectAreaLightData[i].PositionVS[numVert] = XMFLOAT3(Vector3(positionVS));
			}
		}
		CommandContext::InitializeBuffer(m_LightBuffer, m_LightData, MaxLights * sizeof(LightData));
		CommandContext::InitializeBuffer(m_GlobalRectAreaLightData, m_RectAreaLightData, MaxAreaLights * sizeof(RectAreaLightData));
	}
}

void Lighting::UpdateLightingDataChange(bool IsChanged)
{
	bLightDataChanged = IsChanged;
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
	m_GlobalRectAreaLightData.Destroy();
}

void Lighting::InitMaterial()
{
	m_FillLightGridCS_8 = RenderMaterialManager::GetInstance().GetMaterial("Fill Light Grid 8 CS", MaterialPipelineType::Compute);
	m_FillLightGridCS_16 = RenderMaterialManager::GetInstance().GetMaterial("Fill Light Grid 16 CS", MaterialPipelineType::Compute);
	m_FillLightGridCS_24 = RenderMaterialManager::GetInstance().GetMaterial("Fill Light Grid 24 CS", MaterialPipelineType::Compute);
	m_FillLightGridCS_32 = RenderMaterialManager::GetInstance().GetMaterial("Fill Light Grid 36 CS", MaterialPipelineType::Compute);

	m_BuildClusterCS = RenderMaterialManager::GetInstance().GetMaterial("Build Cluster CS", MaterialPipelineType::Compute);
	m_MaskUnUsedClusterCS = RenderMaterialManager::GetInstance().GetMaterial("Mask UnUsed Cluster", MaterialPipelineType::Compute);
	m_ClusterLightCullCS = RenderMaterialManager::GetInstance().GetMaterial("Cluster Light Cull", MaterialPipelineType::Compute);

	InitComputeMaterial(m_FillLightRootSig, (*m_FillLightGridCS_8), g_pFillLightGrid_8_CS);
	InitComputeMaterial(m_FillLightRootSig, (*m_FillLightGridCS_16), g_pFillLightGrid_16_CS);
	InitComputeMaterial(m_FillLightRootSig, (*m_FillLightGridCS_24), g_pFillLightGrid_24_CS);
	InitComputeMaterial(m_FillLightRootSig, (*m_FillLightGridCS_32), g_pFillLightGrid_32_CS);
	InitComputeMaterial(m_FillLightRootSig, (*m_BuildClusterCS), g_pBuildClusterCS);
	InitComputeMaterial(m_FillLightRootSig, (*m_MaskUnUsedClusterCS), g_pMaskUnUsedClusterCS);
	InitComputeMaterial(m_FillLightRootSig, (*m_ClusterLightCullCS), g_pClusterLightCullCS);

	AreaLightMaterial = RenderMaterialManager::GetInstance().GetMaterial("Area Light Material");
	if (!AreaLightMaterial->IsInitialized)
	{
		AreaLightMaterial->BindPSOCreateFunc([&](const PSOCommonProperty CommonProperty) {
			D3D12_RASTERIZER_DESC Rasterizer = RasterizerTwoSidedCw;
			D3D12_BLEND_DESC Blend = BlendDisable;

			GraphicsPSO& AreaLightPSO = AreaLightMaterial->GetGraphicsPSO();

			if (CommonProperty.IsWireframe)
			{
				Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
				Rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
			}
			if (CommonProperty.IsMSAA)
			{
				Rasterizer.MultisampleEnable = true;
				Blend.AlphaToCoverageEnable = true;
			}
			AreaLightPSO.SetRootSignature(*CommonProperty.pRootSignature);
			AreaLightPSO.SetRasterizerState(Rasterizer);
			AreaLightPSO.SetBlendState(Blend);
			if (REVERSE_Z)
			{
				AreaLightPSO.SetDepthStencilState(DepthStateReadWriteReversed);
			}
			else
			{
				AreaLightPSO.SetDepthStencilState(DepthStateReadWrite);
			}

			AreaLightPSO.SetSampleMask(0xFFFFFFFF);
			AreaLightPSO.SetInputLayout((UINT)InputLayout::InstancePosNormalTangentTexc.size(), InputLayout::InstancePosNormalTangentTexc.data());
			AreaLightPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			if (CommonProperty.IsWireframe)
			{
				AreaLightPSO.SetPixelShader(g_pWireframePS, sizeof(g_pWireframePS));
				AreaLightPSO.SetDepthStencilState(DepthStateDisabled);
			}
			else
			{
				AreaLightPSO.SetVertexShader(g_pAreaLightMeshVS, sizeof(g_pAreaLightMeshVS));
				AreaLightPSO.SetPixelShader(g_pAreaLightMeshPS, sizeof(g_pAreaLightMeshPS));
			}
			AreaLightPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
			AreaLightPSO.Finalize();
			});

		AreaLightMaterial->BindPSORuntimeModifyFunc([&]() {
			RuntimePSOManager::Get().RegisterPSO(&AreaLightMaterial->GetGraphicsPSO(), GET_SHADER_PATH("Lighting/AreaLightMeshVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
			RuntimePSOManager::Get().RegisterPSO(&AreaLightMaterial->GetGraphicsPSO(), GET_SHADER_PATH("Lighting/AreaLightMeshPS.hlsl"), D3D12_SHVER_PIXEL_SHADER); });

		AreaLightMaterial->IsInitialized = true;
	}
}

void Lighting::RenderAreaLightMesh(GraphicsContext& context)
{
	if (bEnableTiledBaseLight)
	{
		if (bEnableAreaLight)
		{
			ScopedTimer AreaLightScope(L"Area Light Mesh", context);

			//Set Material Data
			//const vector<MaterialConstant>& MaterialData = MaterialManager::GetMaterialInstance()->GetMaterialsConstantBuffer();
			//context.SetDynamicSRV((int)RootSignatureType::eMaterialConstantData, sizeof(MaterialConstant) * MaterialData.size(), MaterialData.data());
			context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());
			mQuadAreaLightModel->Draw(context, &AreaLightMaterial->GetGraphicsPSO());
		}

		if (bEnablePointLight)
		{
			ScopedTimer PointLightScope(L"Point Light Mesh", context);
			context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());
			mQuadPointLightModel->Draw(context, &AreaLightMaterial->GetGraphicsPSO());
		}

		if (bEnableConeLight)
		{
			ScopedTimer PointLightScope(L"Cone Light Mesh", context);
			context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			context.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV());
			mQuadConeLightModel->Draw(context, &AreaLightMaterial->GetGraphicsPSO());
		}
	}
}

void Lighting::DrawUI()
{
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Directional Light", &bEnableDirectionalLight);
		ImGui::Checkbox("Tiled Base Light", &bEnableTiledBaseLight);
		if (bEnableTiledBaseLight)
		{
			ImGui::Checkbox("Point Light", &bEnablePointLight);
			ImGui::Checkbox("Area Light", &bEnableAreaLight);
			ImGui::Checkbox("Area Light Two Side", &bEnableAreaLightTwoSide);
			ImGui::Checkbox("Cone Light", &bEnableConeLight);
		}
	}
}
