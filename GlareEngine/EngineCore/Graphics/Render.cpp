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

namespace GlareEngine
{
	namespace Render
	{
		//Root Signature
		RootSignature gRootSignature;
		//PSO Common Property
		PSOCommonProperty gCommonProperty;
		//Num Frame Resources
		const int gNumFrameResources = 3;

		vector<GraphicsPSO> gModelPSOs;

		vector<GraphicsPSO> gCommonPSOs;

		DescriptorHeap gModelTextureHeap;

		DescriptorHeap gSamplerHeap;
	}



	void Render::Initialize()
	{
		BuildRootSignature();
		BuildPSOs();
	}

	void Render::ShutDown()
	{
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
		//Sky Cube Texture
		gRootSignature[(int)RootSignatureType::eCubeTextures].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, MAXCUBESRVSIZE);
		//Cook BRDF PBR Textures 
		gRootSignature[(int)RootSignatureType::ePBRTextures].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAXCUBESRVSIZE, MAX2DSRVSIZE);
		//Material Constant Data
		gRootSignature[(int)RootSignatureType::eMaterialConstantData].InitAsBufferSRV(1, 1);
		//Instance Constant Data 
		gRootSignature[(int)RootSignatureType::eInstancConstantData].InitAsBufferSRV(0, 1);

		//Static Samplers
		gRootSignature.InitStaticSampler(0, SamplerLinearWrapDesc);
		gRootSignature.InitStaticSampler(1, SamplerAnisoWrapDesc);
		gRootSignature.InitStaticSampler(2, SamplerShadowDesc);
		gRootSignature.InitStaticSampler(3, SamplerLinearClampDesc);
		gRootSignature.InitStaticSampler(4, SamplerVolumeWrapDesc);
		gRootSignature.InitStaticSampler(5, SamplerPointClampDesc);
		gRootSignature.InitStaticSampler(6, SamplerPointBorderDesc);
		gRootSignature.InitStaticSampler(7, SamplerLinearBorderDesc);

		gRootSignature.Finalize(L"Forward Rendering", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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


	void Render::BuildPSOs()
	{
		BuildCommonPSOs(gCommonProperty);
		CSky::BuildPSO(gCommonProperty);
		InstanceModel::BuildPSO(gCommonProperty);
		glTFInstanceModel::BuildPSO(gCommonProperty);
		ShadowMap::BuildPSO(gCommonProperty);
		IBL::BuildPSOs(gCommonProperty);
		Terrain::BuildPSO(gCommonProperty);
	}

}