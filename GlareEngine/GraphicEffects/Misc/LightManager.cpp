#include "LightManager.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Graphics/CommandContext.h"
#include "Engine/Camera.h"
#include "Graphics/BufferManager.h"
#include "Math/VectorMath.h"
#include "Engine/EngineAdjust.h"

//shaders
#include "CompiledShaders/FillLightGrid_8_CS.h"
#include "CompiledShaders/FillLightGrid_16_CS.h"
#include "CompiledShaders/FillLightGrid_24_CS.h"
#include "CompiledShaders/FillLightGrid_32_CS.h"

using namespace GlareEngine::Math;
using namespace GlareEngine;


// Must keep in sync with HLSL
struct LightData
{
	float PositionWS[3];
	float PositionVS[3];
	float RadiusSquare;
	float Color[3];

	uint32_t Type;
	float ConeDir[3];
	float ConeAngles[2];

	float ShadowTextureMatrix[16];
};

enum { eMinLightGridDimension = 8 };

namespace GlareEngine
{
	namespace Lighting
	{
		//light tile size
		IntVar LightGridDimension("Rendering/Forward+/Light Grid Dimension", 16, eMinLightGridDimension, 32, 8);

		//Light RootSignature
		RootSignature m_FillLightRootSig;

		//four tiled light types
		ComputePSO m_FillLightGridCS_8(L"Fill Light Grid 8 CS");
		ComputePSO m_FillLightGridCS_16(L"Fill Light Grid 16 CS");
		ComputePSO m_FillLightGridCS_24(L"Fill Light Grid 24 CS");
		ComputePSO m_FillLightGridCS_32(L"Fill Light Grid 32 CS");

		LightData m_LightData[MaxLights];
		StructuredBuffer m_LightBuffer;
		StructuredBuffer m_LightGrid;

		uint32_t m_FirstConeLight;
		uint32_t m_FirstConeShadowedLight;

		//shadow map size
		enum { eShadowDimension = 512 };

		ColorBuffer m_LightShadowArray;
		ShadowBuffer m_LightShadowTempBuffer;
		Matrix4 m_LightShadowMatrix[MaxLights];

	}
}

void Lighting::InitializeResources(void)
{
	m_FillLightRootSig.Reset(3, 0);
	m_FillLightRootSig[0].InitAsConstantBuffer(0);
	m_FillLightRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
	m_FillLightRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_FillLightRootSig.Finalize(L"FillLightRS");

	m_FillLightGridCS_8.SetRootSignature(m_FillLightRootSig);
	m_FillLightGridCS_8.SetComputeShader(g_pFillLightGrid_8_CS, sizeof(g_pFillLightGrid_8_CS));
	m_FillLightGridCS_8.Finalize();

	m_FillLightGridCS_16.SetRootSignature(m_FillLightRootSig);
	m_FillLightGridCS_16.SetComputeShader(g_pFillLightGrid_16_CS, sizeof(g_pFillLightGrid_16_CS));
	m_FillLightGridCS_16.Finalize();

	m_FillLightGridCS_24.SetRootSignature(m_FillLightRootSig);
	m_FillLightGridCS_24.SetComputeShader(g_pFillLightGrid_24_CS, sizeof(g_pFillLightGrid_24_CS));
	m_FillLightGridCS_24.Finalize();

	m_FillLightGridCS_32.SetRootSignature(m_FillLightRootSig);
	m_FillLightGridCS_32.SetComputeShader(g_pFillLightGrid_32_CS, sizeof(g_pFillLightGrid_32_CS));
	m_FillLightGridCS_32.Finalize();

	// Assumes max resolution of 3840x2160
	uint32_t lightGridCells = Math::DivideByMultiple(3840, eMinLightGridDimension) * Math::DivideByMultiple(2160, eMinLightGridDimension);
	uint32_t lightGridSizeBytes = lightGridCells * (1 + MaxLights);
	m_LightGrid.Create(L"m_LightGrid", lightGridSizeBytes, sizeof(UINT));

	m_LightShadowArray.CreateArray(L"m_LightShadowArray", eShadowDimension, eShadowDimension, 1, MaxShadowedLights, DXGI_FORMAT_R16_UNORM);
	m_LightShadowTempBuffer.Create(L"m_LightShadowTempBuffer", eShadowDimension, eShadowDimension);

	m_LightBuffer.Create(L"m_LightBuffer", MaxLights, sizeof(LightData));

}

void Lighting::CreateRandomLights(const Vector3 minBound, const Vector3 maxBound)
{

}

void Lighting::FillLightGrid(GraphicsContext& gfxContext, const Camera& camera)
{

}

void Lighting::Shutdown(void)
{
	m_LightBuffer.Destroy();
	m_LightGrid.Destroy();
	m_LightShadowArray.Destroy();
	m_LightShadowTempBuffer.Destroy();
}
