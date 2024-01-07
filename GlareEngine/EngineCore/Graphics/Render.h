#pragma once
#include <vector>
#include "Graphics/GPUBuffer.h"
#include "Math/VectorMath.h"
#include "Engine/Camera.h"
#include "Graphics/CommandContext.h"
#include "Graphics/UploadBuffer.h"
#include "Graphics/TextureManager.h"
#include "Engine/EngineConfig.h"
#include "Engine/Tools/RuntimePSOManager.h"
#include "Misc/ConstantBuffer.h"

namespace GlareEngine
{
	class RootSignature;
	class DescriptorHeap;
	class GraphicsPSO;
	struct PSOCommonProperty;

	namespace Render
	{
		enum  AntiAliasingType :int
		{
			MSAA,
			FXAA,
			TAA,
			TFAA, //TAA AND FXAA
			NOAA
		};

		enum  RenderPipelineType :int
		{
			TBFR,		//Tiled Based Forward Rendering
			TBDR,		//Tiled Based Deferred Rendering
			CBDR,		//Cluster Based Deferred Rendering
			CBFR,		//Cluster Based Forward Rendering
			FR			//Forward Rendering
		};

		enum GBufferType :int
		{
			GBUFFER_Emissive,		//R11G11B10_FLOAT		Emissive(rgb)
			GBUFFER_Normal,			//R10G10B10A2			Normal(rgb)
			GBUFFER_MSR,			//R8G8B8A8				Metallic(r), Specular(g), Roughness(b), ShadingModelID(a)
			GBUFFER_BaseColor,		//R8G8B8A8				BaseColor(rgb), Material AO(a)	
			GBUFFER_WorldTangent,	//R8G8B8A8				WorldTangent(rgb), Anisotropic intensity(a)
			GBUFFER_Count
		};


		const DXGI_FORMAT GBufferFormat[] =
		{
			DXGI_FORMAT_R11G11B10_FLOAT,		//GBUFFER_Emissive
			DXGI_FORMAT_R16G16B16A16_UNORM,		//GBUFFER_Normal
			DXGI_FORMAT_R8G8B8A8_UNORM,			//GBUFFER_MSR
			DXGI_FORMAT_R16G16B16A16_UNORM,	    //GBUFFER_BaseColor
			DXGI_FORMAT_R8G8B8A8_UNORM,			//GBUFFER_WorldTangent
		};
	}


	namespace Render
	{
		//Root Signature
		extern RootSignature gRootSignature;
		//PSO Common Property
		extern PSOCommonProperty gCommonProperty;
		//Number Frame Resources
		extern const int gNumFrameResources;

		extern std::vector<GraphicsPSO> gCommonPSOs;

		extern DescriptorHeap gTextureHeap;

		extern DescriptorHeap gSamplerHeap;

		extern BoolVar SeparateZPass;

		extern RenderPipelineType gRenderPipelineType;
	}


	namespace Render
	{
		//Initialize Engine Render
		void Initialize(ID3D12GraphicsCommandList* CommandList, const Camera& camera);
		void ShutDown();

		uint8_t GetPSO(uint16_t psoFlags);
		void BuildRootSignature();
		void BuildPSOs();
		void BuildCommonPSOs(const PSOCommonProperty CommonProperty);
		void CopyTextureHeap();
		void CopyBufferDescriptors(const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptors, uint32_t Size, const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptors);

		//Deferred rendering
		D3D12_CPU_DESCRIPTOR_HANDLE* GetGBufferRTV(GraphicsContext& context);
		void ClearGBuffer(GraphicsContext& context);
		void RenderDeferredLighting(GraphicsContext& context, MainConstants constants);

		uint32_t GetCommonSRVHeapOffset();
		void SetCommonSRVHeapOffset(int Offset);

		//Render setting
		void SetAntiAliasingType(AntiAliasingType type);
		AntiAliasingType GetAntiAliasingType();
	}
}
