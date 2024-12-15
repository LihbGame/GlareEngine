#include "Render.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GraphicsCommon.h"
#include "SamplerManager.h"

#include "Sky/Sky.h"
#include "Shadow/ShadowMap.h"
#include "Terrain/Terrain.h"
#include "GI/IBL.h"
#include "InstanceModel/InstanceModel.h"
#include "InstanceModel/glTFInstanceModel.h"
#include "PostProcessing/PostProcessing.h"
#include "Misc/LightManager.h"
#include "PostProcessing/SSAO.h"
#include "Engine/RenderMaterial.h"
#include "Engine/EngineProfiling.h"
#include "Engine/EngineGlobal.h"
#include "Engine/Scene.h"



#include "CompiledShaders/LightingPassCS.h"
#include "CompiledShaders/WireframeCS.h"

#define MAX_TEXTURE_HEAP_DESCRIPTORS  4096

namespace GlareEngine
{
	namespace Render
	{
		BoolVar SeparateZPass("Renderer/Separate Z Pass", true);

		bool s_Initialized = false;

		//Root Signature
		RootSignature gRootSignature;

		//PSO Common Property
		PSOCommonProperty gCommonProperty;
		//Num Frame Resources
		const int gNumFrameResources = NUMFRAME;

		vector<GraphicsPSO> gCommonPSOs;
		//include UAV AND SRV
		DescriptorHeap gTextureHeap;

		DescriptorHeap gSamplerHeap;

		uint32_t CopyDescriptorsRangesSize[MAX_TEXTURE_HEAP_DESCRIPTORS];

		uint32_t CommonSRVHeapOffset = 0;

		//Rendering Setting
		AntiAliasingType gAntiAliasingType = AntiAliasingType::MSAA;

		RenderPipelineType gRenderPipelineType = RenderPipelineType::TBFR;

		D3D12_CPU_DESCRIPTOR_HANDLE g_GBufferSRV[GBUFFER_Count];

		RenderMaterial* DeferredLightingMaterial = nullptr;
		RenderMaterial* WireFrameMaterial = nullptr;

	}

	void Render::Initialize(ID3D12GraphicsCommandList* CommandList,const Camera& camera)
	{
		if (s_Initialized)
			return;

		Lighting::InitializeResources(camera);
		ScreenProcessing::Initialize(CommandList);

		BuildRootSignature();
		BuildCommonPSOs(gCommonProperty);
		BuildPSOs();

#if USE_RUNTIME_PSO
		//After build PSOs
		BuildRuntimePSOs();

		PRTManager::InitRuntimePSO();
		Lighting::InitRuntimePSO();
#endif
		for (UINT index = 0; index < MAX_TEXTURE_HEAP_DESCRIPTORS; index++)
		{
			CopyDescriptorsRangesSize[index] = 1;
		}

		gTextureHeap.Create(L"Scene Texture Descriptors", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_TEXTURE_HEAP_DESCRIPTORS);

		gSamplerHeap.Create(L"Scene Sampler Descriptors", D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048);

#if	USE_RUNTIME_PSO
		RuntimePSOManager::Get().RegisterPSO(&DeferredLightingMaterial->GetComputePSO(), GET_SHADER_PATH("Lighting/LightingPassCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
		RuntimePSOManager::Get().RegisterPSO(&WireFrameMaterial->GetComputePSO(), GET_SHADER_PATH("Misc/WireframeCS.hlsl"), D3D12_SHVER_COMPUTE_SHADER);
#endif

		SSAO::Initialize();

		s_Initialized = true;
	}

	void Render::ShutDown()
	{
		Lighting::Shutdown();
		gTextureHeap.Destroy();
		gSamplerHeap.Destroy();
	}

	void Render::BuildRootSignature()
	{
		gRootSignature.Reset((int)RootSignatureType::eRootSignatureSize, 8);
		//Main Constant Buffer
		gRootSignature[(int)RootSignatureType::eMainConstantBuffer].InitAsConstantBuffer(0);
		//Common Constant Buffer
			//1.Objects Constant Buffer
			//2.Shadow  Constant Buffer
			//3.Terrain Constant buffer
		gRootSignature[(int)RootSignatureType::eCommonConstantBuffer].InitAsConstantBuffer(1);
		//Material Constants 
		gRootSignature[(int)RootSignatureType::eMaterialConstants].InitAsConstantBuffer(2);
		//Common SRV 
		gRootSignature[(int)RootSignatureType::eCommonSRVs].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, COMMONSRVSIZE);
		//Common UAV
		gRootSignature[(int)RootSignatureType::eCommonUAVs].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, COMMONUAVSIZE);
		//PBR Material SRVs 
		gRootSignature[(int)RootSignatureType::eMaterialSRVs].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, COMMONSRVSIZE, MAXPBRSRVSIZE);
		//Sky Cube Texture
		gRootSignature[(int)RootSignatureType::eCubeTextures].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, COMMONSRVSIZE+MAXPBRSRVSIZE, MAXCUBESRVSIZE);
		//Cook 2D And other PBR Textures 
		gRootSignature[(int)RootSignatureType::ePBRTextures].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAXCUBESRVSIZE+ MAXPBRSRVSIZE+ COMMONSRVSIZE, MAX2DSRVSIZE);
		//Material Samplers
		gRootSignature[(int)RootSignatureType::eMaterialSamplers].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 10, D3D12_SHADER_VISIBILITY_PIXEL);
		//Instance Constant Data 
		gRootSignature[(int)RootSignatureType::eInstanceConstantData].InitAsBufferSRV(0, 1);
		//Material Constant Data
		gRootSignature[(int)RootSignatureType::eMaterialConstantData].InitAsBufferSRV(1, 1);
		//Skin Matrices 
		gRootSignature[(int)RootSignatureType::eSkinMatrices].InitAsBufferSRV(2, 1);


		//Static Samplers
		gRootSignature.InitStaticSampler(10, SamplerLinearWrapDesc);
		gRootSignature.InitStaticSampler(11, SamplerAnisoWrapDesc);
		gRootSignature.InitStaticSampler(12, SamplerShadowDesc);
		gRootSignature.InitStaticSampler(13, SamplerLinearClampDesc);
		gRootSignature.InitStaticSampler(14, SamplerVolumeWrapDesc);
		gRootSignature.InitStaticSampler(15, SamplerPointClampDesc);
		gRootSignature.InitStaticSampler(16, SamplerPointBorderDesc);
		gRootSignature.InitStaticSampler(17, SamplerLinearBorderDesc);

		gRootSignature.Finalize(L"Root Signature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		gCommonProperty.pRootSignature = &gRootSignature;
	}

	uint8_t Render::GetPSO(uint16_t psoFlags)
	{
		return uint8_t();
	}

	void Render::BuildCommonPSOs(const PSOCommonProperty CommonProperty)
	{
		DeferredLightingMaterial = RenderMaterialManager::GetInstance().GetMaterial("Deferred Light CS",MaterialPipelineType::Compute);
		WireFrameMaterial = RenderMaterialManager::GetInstance().GetMaterial("Deferred WireFrame CS", MaterialPipelineType::Compute);
		InitComputeMaterial(gRootSignature, (*DeferredLightingMaterial), g_pLightingPassCS);
		InitComputeMaterial(gRootSignature, (*WireFrameMaterial), g_pWireframeCS);

		//assert(gCommonPSOs.size() == 0);

		//// Depth Only PSOs

		//GraphicsPSO DepthOnlyPSO(L"Render: Depth Only PSO");
		//DepthOnlyPSO.SetRootSignature(gRootSignature);
		//DepthOnlyPSO.SetRasterizerState(RasterizerDefault);
		//DepthOnlyPSO.SetBlendState(BlendDisable);
		//DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
		//DepthOnlyPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
		//DepthOnlyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		//DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, DSV_FORMAT);
		//DepthOnlyPSO.SetVertexShader(g_pDepthOnlyVS, sizeof(g_pDepthOnlyVS));
		//DepthOnlyPSO.Finalize();
		//gPSOs.push_back(DepthOnlyPSO);

	}

	void Render::CopyTextureHeap()
	{
		UINT CubeTextureSize = g_CubeSRV.size();
		D3D12_CPU_DESCRIPTOR_HANDLE CubeSRV[MAXCUBESRVSIZE];
		UINT CubeSrcTextureSize[MAXCUBESRVSIZE];
		for (size_t CubeSRVIndex = 0; CubeSRVIndex < CubeTextureSize; CubeSRVIndex++)
		{
			CubeSRV[CubeSRVIndex] = g_CubeSRV[CubeSRVIndex];
			CubeSrcTextureSize[CubeSRVIndex] = 1;
		}
		g_Device->CopyDescriptors(1, &gTextureHeap[COMMONSRVSIZE+COMMONUAVSIZE], &CubeTextureSize,
			CubeTextureSize, CubeSRV, CubeSrcTextureSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		UINT Texture2DSize = g_TextureSRV.size();
		D3D12_CPU_DESCRIPTOR_HANDLE Texture2DSRV[MAX2DSRVSIZE];
		UINT Texture2DSrcSize[MAX2DSRVSIZE];
		for (size_t Texture2DSRVIndex = 0; Texture2DSRVIndex < Texture2DSize; Texture2DSRVIndex++)
		{
			Texture2DSRV[Texture2DSRVIndex] = g_TextureSRV[Texture2DSRVIndex];
			Texture2DSrcSize[Texture2DSRVIndex] = 1;
		}
		g_Device->CopyDescriptors(1, &gTextureHeap[MAXCUBESRVSIZE+ COMMONSRVSIZE+ COMMONUAVSIZE], &Texture2DSize,
			Texture2DSize, Texture2DSRV, Texture2DSrcSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	uint32_t Render::GetCommonSRVHeapOffset()
	{
		return CommonSRVHeapOffset;
	}

	void Render::SetCommonSRVHeapOffset(int Offset)
	{
		assert(Offset < COMMONSRVSIZE);
		CommonSRVHeapOffset = Offset;
	}

	void Render::CopyBufferDescriptors(const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptors, uint32_t Size,const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptors)
	{
		UINT destCount = Size;
		g_Device->CopyDescriptors(1, pDestDescriptors, &destCount,
			Size, pSrcDescriptors, CopyDescriptorsRangesSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE* Render::GetGBufferRTV(GraphicsContext& context)
	{
		for (int i = 0; i < GBUFFER_Count; ++i)
		{
			g_GBufferSRV[i] = g_GBuffer[i].GetRTV();
			context.TransitionResource(g_GBuffer[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		return g_GBufferSRV;
	}

	void Render::ClearGBuffer(GraphicsContext& context)
	{
		for (int i = 0; i < GBUFFER_Count; ++i)
		{
			context.TransitionResource(g_GBuffer[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
			context.ClearColor(g_GBuffer[i]);
		}
	}

	void Render::RenderDeferredLighting(GraphicsContext& context, MainConstants constants)
	{
		ComputeContext& Context = context.GetComputeContext();

		ScopedTimer RenderLightingScope(L"Render Deferred Lighting", Context);

		Context.SetRootSignature(gRootSignature);
		if (EngineGlobal::gCurrentScene->IsWireFrame)
		{
			Context.SetPipelineState(WireFrameMaterial->GetRuntimePSO());
		}
		else
		{
			Context.SetPipelineState(DeferredLightingMaterial->GetRuntimePSO());
		}
		//set descriptor heap 
		Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());

		//Copy GBuffer SRV
		{
			D3D12_CPU_DESCRIPTOR_HANDLE GBUFFER_SRV[] =
			{
			g_GBuffer[GBUFFER_Emissive].GetSRV(),
			g_GBuffer[GBUFFER_Normal].GetSRV(),
			g_GBuffer[GBUFFER_MSR].GetSRV(),
			g_GBuffer[GBUFFER_BaseColor].GetSRV(),
			g_GBuffer[GBUFFER_WorldTangent].GetSRV(),
			g_SceneDepthBuffer.GetDepthSRV()
			};

			CopyBufferDescriptors(GBUFFER_SRV, ArraySize(GBUFFER_SRV), &gTextureHeap[0]);
		}

		{
			D3D12_CPU_DESCRIPTOR_HANDLE Scene_UAV[] =
			{
			g_SceneColorBuffer.GetUAV()
			};

			CopyBufferDescriptors(Scene_UAV, ArraySize(Scene_UAV), &gTextureHeap[COMMONSRVSIZE]);
		}


		Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
		Context.TransitionResource(g_GBuffer[GBUFFER_Emissive], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_GBuffer[GBUFFER_Normal], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_GBuffer[GBUFFER_MSR], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_GBuffer[GBUFFER_BaseColor], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_GBuffer[GBUFFER_WorldTangent], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


		Context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &constants);
		//Set Common SRV
		Context.SetDescriptorTable((int)RootSignatureType::eCommonSRVs, gTextureHeap[0]);
		//Set Cube SRV
		Context.SetDescriptorTable((int)RootSignatureType::eCubeTextures, gTextureHeap[COMMONSRVSIZE+ COMMONUAVSIZE]);
		//Set Textures SRV
		Context.SetDescriptorTable((int)RootSignatureType::ePBRTextures, gTextureHeap[MAXCUBESRVSIZE + COMMONSRVSIZE+ COMMONUAVSIZE]);
		 
		Context.SetDescriptorTable((int)RootSignatureType::eCommonUAVs, gTextureHeap[COMMONSRVSIZE]);
		
		Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight());
	}

	void Render::SetAntiAliasingType(AntiAliasingType type)
	{
		gAntiAliasingType = type;
	}

	Render::AntiAliasingType Render::GetAntiAliasingType()
	{
		return gAntiAliasingType;
	}


	void Render::BuildPSOs()
	{
		RenderMaterialManager::GetInstance().BuildMaterialsPSO(gCommonProperty);

		PRTManager::BuildPSO(gCommonProperty);
		Lighting::BuildPSO(gCommonProperty);
		glTFInstanceModel::BuildPSO(gCommonProperty);
		IBL::BuildPSOs(gCommonProperty);
		Terrain::BuildPSO(gCommonProperty);
		ScreenProcessing::BuildPSO(gCommonProperty);
	}

	void Render::BuildRuntimePSOs()
	{
		RenderMaterialManager::GetInstance().InitRuntimePSO();
	}

}