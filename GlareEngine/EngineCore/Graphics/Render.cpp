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

		DescriptorHeap gTextureHeap;

		DescriptorHeap gSamplerHeap;

		//Rendering Setting
		AntiAliasingType gAntiAliasingType = AntiAliasingType::MSAA;

		RenderPipelineType gRenderPipelineType = RenderPipelineType::TBFR;
	}

	void Render::Initialize(ID3D12GraphicsCommandList* CommandList)
	{
		if (s_Initialized)
			return;

		BuildRootSignature();
		BuildPSOs();

#if USE_RUNTIME_PSO
		//After build pso
		InstanceModel::InitRuntimePSO();
		ShadowMap::InitRuntimePSO();
#endif

		gTextureHeap.Create(L"Scene Texture Descriptors", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096);

		gSamplerHeap.Create(L"Scene Sampler Descriptors", D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048);

		Lighting::InitializeResources();

		ScreenProcessing::Initialize(CommandList);

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
		g_Device->CopyDescriptors(1, &gTextureHeap[COMMONSRVSIZE], &CubeTextureSize,
			CubeTextureSize, CubeSRV, CubeSrcTextureSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		UINT Texture2DSize = g_TextureSRV.size();
		D3D12_CPU_DESCRIPTOR_HANDLE Texture2DSRV[MAX2DSRVSIZE];
		UINT Texture2DSrcSize[MAX2DSRVSIZE];
		for (size_t Texture2DSRVIndex = 0; Texture2DSRVIndex < Texture2DSize; Texture2DSRVIndex++)
		{
			Texture2DSRV[Texture2DSRVIndex] = g_TextureSRV[Texture2DSRVIndex];
			Texture2DSrcSize[Texture2DSRVIndex] = 1;
		}
		g_Device->CopyDescriptors(1, &gTextureHeap[MAXCUBESRVSIZE+ COMMONSRVSIZE], &Texture2DSize,
			Texture2DSize, Texture2DSRV, Texture2DSrcSize, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
		BuildCommonPSOs(gCommonProperty);
		CSky::BuildPSO(gCommonProperty);
		InstanceModel::BuildPSO(gCommonProperty);
		glTFInstanceModel::BuildPSO(gCommonProperty);
		ShadowMap::BuildPSO(gCommonProperty);
		IBL::BuildPSOs(gCommonProperty);
		Terrain::BuildPSO(gCommonProperty);
		ScreenProcessing::BuildPSO(gCommonProperty);
	}

}