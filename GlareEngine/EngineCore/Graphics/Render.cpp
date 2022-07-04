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
		gRootSignature.Reset(6, 8);
		//Main Constant Buffer
		gRootSignature[(int)RootSignatureType::MainConstantBuffer].InitAsConstantBuffer(0);
		//Common Constant Buffer
			//1.Objects Constant Buffer
			//2.Shadow  Constant Buffer
			//3.Terrain Constant buffer
		gRootSignature[(int)RootSignatureType::CommonConstantBuffer].InitAsConstantBuffer(1);
		//Sky Cube Texture
		gRootSignature[(int)RootSignatureType::CubeTextures].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, MAXCUBESRVSIZE);
		//Cook BRDF PBR Textures 
		gRootSignature[(int)RootSignatureType::PBRTextures].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAXCUBESRVSIZE, MAX2DSRVSIZE);
		//Material Constant Data
		gRootSignature[(int)RootSignatureType::MaterialConstantData].InitAsBufferSRV(1, 1);
		//Instance Constant Data 
		gRootSignature[(int)RootSignatureType::InstancConstantData].InitAsBufferSRV(0, 1);

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

	void Render::BuildPSOs()
	{
		CSky::BuildPSO(gCommonProperty);
		InstanceModel::BuildPSO(gCommonProperty);
		ShadowMap::BuildPSO(gCommonProperty);
		IBL::BuildPSOs(gCommonProperty);
		Terrain::BuildPSO(gCommonProperty);
	}

}