#include "glTFInstanceModel.h"
#include "Graphics/Render.h"
#include "Model/Model.h"
#include "Shadow/ShadowMap.h"
#include <stdlib.h>

//shaders
#include "CompiledShaders/DepthOnlyVS.h"
#include "CompiledShaders/CutoutDepthVS.h"
#include "CompiledShaders/CutoutDepthPS.h"
#include "CompiledShaders/DepthOnlySkinVS.h"
#include "CompiledShaders/CutoutDepthSkinVS.h"
#include "CompiledShaders/ModelPS.h"
#include "CompiledShaders/ModelVS.h"
#include "CompiledShaders/ModelSkinVS.h"
#include "CompiledShaders/ModelNoUV1PS.h"
#include "CompiledShaders/ModelNoUV1VS.h"
#include "CompiledShaders/ModelNoUV1SkinVS.h"
#include "CompiledShaders/ModelNoTangentPS.h"
#include "CompiledShaders/ModelNoTangentSkinVS.h"
#include "CompiledShaders/ModelNoTangentNoUV1PS.h"
#include "CompiledShaders/ModelNoTangentNoUV1SkinVS.h"
#include "CompiledShaders/ModelNoTangentNoUV1VS.h"
#include "CompiledShaders/ModelNoTangentVS.h"
#include "CompiledShaders/WireframePS.h"
#include "CompiledShaders/CutoutDepthMSAAPS.h"
#include "CompiledShaders/DeferredModelPS.h"
#include "CompiledShaders/DeferredModelNoUV1PS.h"

using namespace GlareEngine::Render;
using namespace GlareEngine::ModelPSOFlags;

vector<GraphicsPSO> glTFInstanceModel::gModelPSOs;
GraphicsPSO glTFInstanceModel::sm_PBRglTFPSO;
PSOCommonProperty glTFInstanceModel::sm_PSOCommonProperty;
vector<uint16_t> glTFInstanceModel::mPSOFlags;

#if USE_RUNTIME_PSO
vector<glTFInstanceModel::ShaderNames> glTFInstanceModel::mShaderNames;
#endif

D3D12_RASTERIZER_DESC  glTFInstanceModel::mMSAARasterizer;
D3D12_RASTERIZER_DESC  glTFInstanceModel::mRasterizer;
D3D12_BLEND_DESC  glTFInstanceModel::mCoverageBlend;
D3D12_BLEND_DESC  glTFInstanceModel::mBlend;
std::mutex glTFInstanceModel::m_PSOMutex;

#define MAX_PSO 256

enum ModelPSO:int
{
    DepthOnlyPSO,
    CutoutDepthPSO,
    SkinDepthOnlyPSO,
    SkinCutoutDepthPSO,
    ShadowDepthOnlyPSO,
    ShadowCutoutDepthPSO,
    ShadowSkinDepthOnlyPSO,
    ShadowSkinCutoutDepthPSO,
    PSOCount
};


void glTFInstanceModel::BuildPSO(const PSOCommonProperty CommonProperty)
{
    std::lock_guard<std::mutex> LockGuard(m_PSOMutex);

    sm_PSOCommonProperty = CommonProperty;

    if (gModelPSOs.size() == 0)
    {
        gModelPSOs.resize(ModelPSO::PSOCount);
        gModelPSOs.reserve(MAX_PSO);
    }
	// Depth Only PSOs
	GraphicsPSO DepthOnlyPSO(L"Render: Depth Only PSO");
	DepthOnlyPSO.SetRootSignature(gRootSignature);

	if (CommonProperty.IsMSAA)
	{
		DepthOnlyPSO.SetRasterizerState(RasterizerDefaultMsaa);
	}
	else
	{
		DepthOnlyPSO.SetRasterizerState(RasterizerDefault);
	}
    DepthOnlyPSO.SetBlendState(BlendDisable);

    if (REVERSE_Z)
    {
        DepthOnlyPSO.SetDepthStencilState(DepthStateReadWriteReversed);
    }
    else
    {
        DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
    }
	DepthOnlyPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	DepthOnlyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, g_SceneDepthBuffer.GetFormat(),CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	DepthOnlyPSO.SetVertexShader(g_pDepthOnlyVS, sizeof(g_pDepthOnlyVS));
	DepthOnlyPSO.Finalize();
    gModelPSOs[ModelPSO::DepthOnlyPSO] = DepthOnlyPSO;
   

	//Cutout Depth PSO
	GraphicsPSO CutoutDepthPSO(L"Render: Cutout Depth PSO");
    CutoutDepthPSO.RuntimePSOCopy(DepthOnlyPSO);
	CutoutDepthPSO.SetInputLayout((UINT)InputLayout::PosUV.size(), InputLayout::PosUV.data());
	if (CommonProperty.IsMSAA)
	{
        CutoutDepthPSO.SetBlendState(BlendDisableAlphaToCoverage);
		CutoutDepthPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
	}
	else
	{
        CutoutDepthPSO.SetBlendState(BlendDisable);
		CutoutDepthPSO.SetRasterizerState(RasterizerTwoSided);
	}
	CutoutDepthPSO.SetVertexShader(g_pCutoutDepthVS, sizeof(g_pCutoutDepthVS));
    if (CommonProperty.IsMSAA)
    {
        CutoutDepthPSO.SetPixelShader(g_pCutoutDepthMSAAPS, sizeof(g_pCutoutDepthMSAAPS));
    }
    else
    {
        CutoutDepthPSO.SetPixelShader(g_pCutoutDepthPS, sizeof(g_pCutoutDepthPS));
    }
	CutoutDepthPSO.Finalize();
    gModelPSOs[ModelPSO::CutoutDepthPSO] = CutoutDepthPSO;


	//Skin Depth PSO
	GraphicsPSO SkinDepthOnlyPSO(L"Render: Skin Depth PSO");
	SkinDepthOnlyPSO.RuntimePSOCopy(DepthOnlyPSO);
	SkinDepthOnlyPSO.SetInputLayout((UINT)InputLayout::SkinPos.size(), InputLayout::SkinPos.data());
	SkinDepthOnlyPSO.SetVertexShader(g_pDepthOnlySkinVS, sizeof(g_pDepthOnlySkinVS));
	SkinDepthOnlyPSO.Finalize();
    gModelPSOs[ModelPSO::SkinDepthOnlyPSO] = SkinDepthOnlyPSO;

	//Skin Cutout Depth PSO
	GraphicsPSO SkinCutoutDepthPSO(L"Render: Skin Cutout Depth PSO");
	SkinCutoutDepthPSO.RuntimePSOCopy(CutoutDepthPSO);
	SkinCutoutDepthPSO.SetInputLayout((UINT)InputLayout::SkinPosUV.size(), InputLayout::SkinPosUV.data());
	SkinCutoutDepthPSO.SetVertexShader(g_pCutoutDepthSkinVS, sizeof(g_pCutoutDepthSkinVS));
	SkinCutoutDepthPSO.Finalize();
    gModelPSOs[ModelPSO::SkinCutoutDepthPSO] = SkinCutoutDepthPSO;

	// Shadow PSOs
	DepthOnlyPSO.SetRasterizerState(RasterizerShadow);
    DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
	DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	DepthOnlyPSO.Finalize();
    gModelPSOs[ModelPSO::ShadowDepthOnlyPSO] = DepthOnlyPSO;

	CutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
    CutoutDepthPSO.SetDepthStencilState(DepthStateReadWrite);
	CutoutDepthPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	CutoutDepthPSO.Finalize();
    gModelPSOs[ModelPSO::ShadowCutoutDepthPSO] = CutoutDepthPSO;

	SkinDepthOnlyPSO.SetRasterizerState(RasterizerShadow);
	SkinDepthOnlyPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	SkinDepthOnlyPSO.Finalize();
    gModelPSOs[ModelPSO::ShadowSkinDepthOnlyPSO] = SkinDepthOnlyPSO;

	SkinCutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
	SkinCutoutDepthPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	SkinCutoutDepthPSO.Finalize();
    gModelPSOs[ModelPSO::ShadowSkinCutoutDepthPSO] = SkinCutoutDepthPSO;


#if USE_RUNTIME_PSO
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::DepthOnlyPSO], GET_SHADER_PATH("PBRInstanceModel/DepthOnlyVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::CutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::SkinDepthOnlyPSO], GET_SHADER_PATH("PBRInstanceModel/DepthOnlySkinVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::SkinCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthSkinVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowDepthOnlyPSO], GET_SHADER_PATH("PBRInstanceModel/DepthOnlyVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowSkinDepthOnlyPSO], GET_SHADER_PATH("PBRInstanceModel/DepthOnlySkinVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowSkinCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthSkinVS.hlsl"), D3D12_SHVER_VERTEX_SHADER);
    if (CommonProperty.IsMSAA)
    {
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::CutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthMSAAPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::SkinCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthMSAAPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthMSAAPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowSkinCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthMSAAPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
    }
    else
    {
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::CutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::SkinCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
        RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[ModelPSO::ShadowSkinCutoutDepthPSO], GET_SHADER_PATH("PBRInstanceModel/CutoutDepthPS.hlsl"), D3D12_SHVER_PIXEL_SHADER);
    }

#endif


	// Default PSO
	sm_PBRglTFPSO.SetRootSignature(gRootSignature);


    D3D12_RASTERIZER_DESC Rasterizer = RasterizerDefault;
    if (CommonProperty.IsWireframe)
    {
        Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
        Rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    }

    if (CommonProperty.IsMSAA)
    {
        Rasterizer.MultisampleEnable = true;
        sm_PBRglTFPSO.SetRasterizerState(Rasterizer);
        sm_PBRglTFPSO.SetBlendState(BlendDisable);
    }
    else
    {
        sm_PBRglTFPSO.SetRasterizerState(Rasterizer);
        sm_PBRglTFPSO.SetBlendState(BlendDisable);
    }

    if (REVERSE_Z)
    {
        sm_PBRglTFPSO.SetDepthStencilState(DepthStateReadWriteReversed);
    }
    else
    {
        sm_PBRglTFPSO.SetDepthStencilState(DepthStateReadWrite);
    }

    //set Input layout when you get this PSO.
	sm_PBRglTFPSO.SetInputLayout(0, nullptr);
	sm_PBRglTFPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	sm_PBRglTFPSO.SetRenderTargetFormats(1, &DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	sm_PBRglTFPSO.SetVertexShader(g_pModelVS, sizeof(g_pModelVS));
	sm_PBRglTFPSO.SetPixelShader(g_pModelPS, sizeof(g_pModelPS));


    for (int PSOIndex = ModelPSO::PSOCount; PSOIndex < gModelPSOs.size(); PSOIndex += 2)
    {
        int PSOReadWriteDepthIndex = PSOIndex;
        int PSOEqualDepthIndex = PSOIndex + 1;
        int PSOFlagIndex = (PSOIndex - ModelPSO::PSOCount) / 2;
        if (gModelPSOs.size() > ModelPSO::PSOCount && gModelPSOs[PSOReadWriteDepthIndex].GetPipelineStateObject() != nullptr)
        {
            if (mPSOFlags[PSOFlagIndex] & eHasSkin)
            {
                if (mPSOFlags[PSOFlagIndex] & eHasTangent)
                {
                    if (mPSOFlags[PSOFlagIndex] & eHasUV1)
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelSkinVS, sizeof(g_pModelSkinVS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelSkinVS, sizeof(g_pModelSkinVS));
                        
                        if (Render::gRenderPipelineType == RenderPipelineType::TBFR||
                            Render::gRenderPipelineType ==RenderPipelineType::FR)
                        {
                            gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
                            gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/ModelPS.hlsl");
#endif
                        }
                        else if (Render::gRenderPipelineType == RenderPipelineType::TBDR)
                        {
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/DeferredModelPS.hlsl");
#endif
                        }
                    }
                    else
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelNoUV1SkinVS, sizeof(g_pModelNoUV1SkinVS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelNoUV1SkinVS, sizeof(g_pModelNoUV1SkinVS));
                       
						if (Render::gRenderPipelineType == RenderPipelineType::TBFR ||
							Render::gRenderPipelineType == RenderPipelineType::FR)
						{
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoUV1PS.hlsl");
#endif
                        }
						else if (Render::gRenderPipelineType == RenderPipelineType::TBDR)
						{
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pDeferredModelNoUV1PS, sizeof(g_pDeferredModelNoUV1PS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pDeferredModelNoUV1PS, sizeof(g_pDeferredModelNoUV1PS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/DeferredModelNoUV1PS.hlsl");
#endif
                        }
                    
                    }
                }
                else
                {
                    if (mPSOFlags[PSOFlagIndex] & eHasUV1)
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelNoTangentSkinVS, sizeof(g_pModelNoTangentSkinVS));
                        gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelNoTangentSkinVS, sizeof(g_pModelNoTangentSkinVS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));
                    }
                    else
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelNoTangentNoUV1SkinVS, sizeof(g_pModelNoTangentNoUV1SkinVS));
                        gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelNoTangentNoUV1SkinVS, sizeof(g_pModelNoTangentNoUV1SkinVS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));
                    }
                }

				if (Render::gRenderPipelineType == RenderPipelineType::TBFR ||
					Render::gRenderPipelineType == RenderPipelineType::FR)
				{
					gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
					gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
				}
				else if (Render::gRenderPipelineType == RenderPipelineType::TBDR)
				{
					gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
					gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
				}
            }
            else
            {
                if (mPSOFlags[PSOFlagIndex] & eHasTangent)
                {
                    if (mPSOFlags[PSOFlagIndex] & eHasUV1)
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelVS, sizeof(g_pModelVS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelVS, sizeof(g_pModelVS));
                    
						if (Render::gRenderPipelineType == RenderPipelineType::TBFR ||
							Render::gRenderPipelineType == RenderPipelineType::FR)
						{
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/ModelPS.hlsl");
#endif
                        }
						else if (Render::gRenderPipelineType == RenderPipelineType::TBDR)
						{
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/DeferredModelPS.hlsl");
#endif	
                        }
                    
                    }
                    else
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelNoUV1VS, sizeof(g_pModelNoUV1VS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelNoUV1VS, sizeof(g_pModelNoUV1VS));
                    
						if (Render::gRenderPipelineType == RenderPipelineType::TBFR ||
							Render::gRenderPipelineType == RenderPipelineType::FR)
						{
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoUV1PS.hlsl");
#endif
                        }
						else if (Render::gRenderPipelineType == RenderPipelineType::TBDR)
						{
							gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pDeferredModelNoUV1PS, sizeof(g_pDeferredModelNoUV1PS));
							gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pDeferredModelNoUV1PS, sizeof(g_pDeferredModelNoUV1PS));
#if USE_RUNTIME_PSO
                            mShaderNames[PSOFlagIndex].PSName = GET_SHADER_PATH("PBRInstanceModel/DeferredModelNoUV1PS.hlsl");
#endif	
                        }
                    }
                }
                else
                {
                    if (mPSOFlags[PSOFlagIndex] & eHasUV1)
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelNoTangentVS, sizeof(g_pModelNoTangentVS));
                        gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelNoTangentVS, sizeof(g_pModelNoTangentVS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));
                    }
                    else
                    {
                        gModelPSOs[PSOEqualDepthIndex].SetVertexShader(g_pModelNoTangentNoUV1VS, sizeof(g_pModelNoTangentNoUV1VS));
                        gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetVertexShader(g_pModelNoTangentNoUV1VS, sizeof(g_pModelNoTangentNoUV1VS));
                        gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));
                    }

					if (Render::gRenderPipelineType == RenderPipelineType::TBFR ||
						Render::gRenderPipelineType == RenderPipelineType::FR)
					{
						gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
						gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pModelPS, sizeof(g_pModelPS));
					}
					else if (Render::gRenderPipelineType == RenderPipelineType::TBDR)
					{
						gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
						gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pDeferredModelPS, sizeof(g_pDeferredModelPS));
					}
                }
            }

			if (Render::gRenderPipelineType == RenderPipelineType::TBFR ||
				Render::gRenderPipelineType == RenderPipelineType::FR)
            {
                gModelPSOs[PSOReadWriteDepthIndex].SetRenderTargetFormats(1, &DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
                gModelPSOs[PSOEqualDepthIndex].SetRenderTargetFormats(1, &DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
            }
            else if(Render::gRenderPipelineType==RenderPipelineType::TBDR)
            {
                gModelPSOs[PSOReadWriteDepthIndex].SetRenderTargetFormats(GBufferType::GBUFFER_Count, Render::GBufferFormat, g_SceneDepthBuffer.GetFormat());
                gModelPSOs[PSOEqualDepthIndex].SetRenderTargetFormats(GBufferType::GBUFFER_Count, Render::GBufferFormat, g_SceneDepthBuffer.GetFormat());
            }


            if (CommonProperty.IsWireframe)
            {
                gModelPSOs[PSOReadWriteDepthIndex].SetDepthStencilState(DepthStateDisabled);
                gModelPSOs[PSOEqualDepthIndex].SetDepthStencilState(DepthStateDisabled);
            }
            else
            {
                if (REVERSE_Z)
                {
                    gModelPSOs[PSOReadWriteDepthIndex].SetDepthStencilState(DepthStateReadOnlyReversed);
                }
                else
                {
                    gModelPSOs[PSOReadWriteDepthIndex].SetDepthStencilState(DepthStateReadOnly);
                }
                gModelPSOs[PSOEqualDepthIndex].SetDepthStencilState(DepthStateTestEqual);
            }

            if (CommonProperty.IsWireframe)
            {
                gModelPSOs[PSOReadWriteDepthIndex].GetPSODesc().RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
                gModelPSOs[PSOReadWriteDepthIndex].GetPSODesc().RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
                gModelPSOs[PSOEqualDepthIndex].GetPSODesc().RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
                gModelPSOs[PSOEqualDepthIndex].GetPSODesc().RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; 
                gModelPSOs[PSOEqualDepthIndex].SetPixelShader(g_pWireframePS, sizeof(g_pWireframePS));
                gModelPSOs[PSOReadWriteDepthIndex].SetPixelShader(g_pWireframePS, sizeof(g_pWireframePS));
            }
            else
            {
                if (mPSOFlags[PSOFlagIndex] & eTwoSided)
                {
                    gModelPSOs[PSOReadWriteDepthIndex].GetPSODesc().RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
                    gModelPSOs[PSOEqualDepthIndex].GetPSODesc().RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
                }
                else
                {
                    gModelPSOs[PSOReadWriteDepthIndex].GetPSODesc().RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
                    gModelPSOs[PSOEqualDepthIndex].GetPSODesc().RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
                }
                gModelPSOs[PSOReadWriteDepthIndex].GetPSODesc().RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
                gModelPSOs[PSOEqualDepthIndex].GetPSODesc().RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            }

            if (mPSOFlags[PSOFlagIndex] & eAlphaBlend)
            {
                gModelPSOs[PSOReadWriteDepthIndex].SetBlendState(BlendTraditional);
                gModelPSOs[PSOEqualDepthIndex].SetBlendState(BlendTraditional);
            }
            else
            {
                gModelPSOs[PSOReadWriteDepthIndex].SetBlendState(BlendDisable);
                gModelPSOs[PSOEqualDepthIndex].SetBlendState(BlendDisable);
            }

            gModelPSOs[PSOReadWriteDepthIndex].Finalize();
            gModelPSOs[PSOEqualDepthIndex].Finalize();

#if USE_RUNTIME_PSO
            RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[PSOReadWriteDepthIndex], mShaderNames[PSOFlagIndex].VSName.c_str(), D3D12_SHVER_VERTEX_SHADER);
            RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[PSOReadWriteDepthIndex], mShaderNames[PSOFlagIndex].PSName.c_str(), D3D12_SHVER_PIXEL_SHADER);
            RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[PSOEqualDepthIndex], mShaderNames[PSOFlagIndex].VSName.c_str(), D3D12_SHVER_VERTEX_SHADER);
            RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[PSOEqualDepthIndex], mShaderNames[PSOFlagIndex].PSName.c_str(), D3D12_SHVER_PIXEL_SHADER);
#endif
        }
    }
}

uint8_t glTFInstanceModel::GetPSO(uint16_t psoFlags)
{
    GraphicsPSO ColorPSO;
    ColorPSO.RuntimePSOCopy(sm_PBRglTFPSO);
#if USE_RUNTIME_PSO
    ShaderNames shaderNames;
#endif

    //Mesh at least have position and normal
    uint16_t Requirements = eHasPosition | eHasNormal;
    assert((psoFlags & Requirements) == Requirements);

    std::vector<D3D12_INPUT_ELEMENT_DESC> vertexLayout;
    if (psoFlags & eHasPosition)
        vertexLayout.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT });
    
    if (psoFlags & eHasNormal)
        vertexLayout.push_back({ "NORMAL",   0, DXGI_FORMAT_R10G10B10A2_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT });
    
    if (psoFlags & eHasTangent)
        vertexLayout.push_back({ "TANGENT",  0, DXGI_FORMAT_R10G10B10A2_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT });
    
    if (psoFlags & eHasUV0)
        vertexLayout.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT });
    else
        vertexLayout.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT });
    
    if (psoFlags & eHasUV1)
        vertexLayout.push_back({ "TEXCOORD", 1, DXGI_FORMAT_R16G16_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT });
    
    if (psoFlags & eHasSkin)
    {
        vertexLayout.push_back({ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
        vertexLayout.push_back({ "BLENDWEIGHT", 0, DXGI_FORMAT_R16G16B16A16_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
    }

    ColorPSO.SetInputLayout((uint32_t)vertexLayout.size(), vertexLayout.data());

    if (psoFlags & eHasSkin)
    {
        if (psoFlags & eHasTangent)
        {
            if (psoFlags & eHasUV1)
            {
                ColorPSO.SetVertexShader(g_pModelSkinVS, sizeof(g_pModelSkinVS));
                ColorPSO.SetPixelShader(g_pModelPS, sizeof(g_pModelPS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelSkinVS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelPS.hlsl");
#endif
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoUV1SkinVS, sizeof(g_pModelNoUV1SkinVS));
                ColorPSO.SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoUV1SkinVS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoUV1PS.hlsl");
#endif
            }
        }
        else
        {
            if (psoFlags & eHasUV1)
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentSkinVS, sizeof(g_pModelNoTangentSkinVS));
                ColorPSO.SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentSkinVS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentPS.hlsl");
#endif
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentNoUV1SkinVS, sizeof(g_pModelNoTangentNoUV1SkinVS));
                ColorPSO.SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentNoUV1SkinVS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentNoUV1PS.hlsl");
#endif
            }
        }
    }
    else
    {
        if (psoFlags & eHasTangent)
        {
            if (psoFlags & eHasUV1)
            {
                ColorPSO.SetVertexShader(g_pModelVS, sizeof(g_pModelVS));
                ColorPSO.SetPixelShader(g_pModelPS, sizeof(g_pModelPS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelVS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelPS.hlsl");
#endif
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoUV1VS, sizeof(g_pModelNoUV1VS));
                ColorPSO.SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoUV1VS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoUV1PS.hlsl");
#endif
            }
        }
        else
        {
            if (psoFlags & eHasUV1)
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentVS, sizeof(g_pModelNoTangentVS));
                ColorPSO.SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentVS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentPS.hlsl");
#endif
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentNoUV1VS, sizeof(g_pModelNoTangentNoUV1VS));
                ColorPSO.SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));

#if USE_RUNTIME_PSO
                shaderNames.VSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentNoUV1VS.hlsl");
                shaderNames.PSName = GET_SHADER_PATH("PBRInstanceModel/ModelNoTangentNoUV1PS.hlsl");
#endif
            }
        }
    }

	mCoverageBlend = BlendDisable;
	mBlend = BlendDisable;

    
    if (psoFlags & eTwoSided)
    {
        if (sm_PSOCommonProperty.IsMSAA)
        {
            ColorPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
            ColorPSO.SetBlendState(BlendDisable);
            mCoverageBlend = BlendDisable;
        }
        else
        {
            ColorPSO.SetRasterizerState(RasterizerTwoSided);
            ColorPSO.SetBlendState(BlendDisable);
			mBlend = BlendDisable;
        }
    }

	if (REVERSE_Z)
	{
		ColorPSO.SetDepthStencilState(DepthStateReadWriteReversed);
	}
	else
	{
		ColorPSO.SetDepthStencilState(DepthStateReadWrite);
	}

    if (psoFlags & eAlphaBlend)
    {
        if(sm_PSOCommonProperty.IsMSAA)
        { 
            ColorPSO.SetRasterizerState(RasterizerDefaultMsaa);
        }
        else
        {
            ColorPSO.SetRasterizerState(RasterizerTwoSided);
        }
        ColorPSO.SetBlendState(BlendTraditional);
        mBlend = BlendTraditional;

		if (REVERSE_Z)
		{
			ColorPSO.SetDepthStencilState(DepthStateReadOnlyReversed);
		}
		else
		{
			ColorPSO.SetDepthStencilState(DepthStateReadOnly);
		}
    }


    ColorPSO.Finalize();

    // Is this PSO exist?
    for (uint32_t i = 0; i < gModelPSOs.size(); ++i)
    {
        if (ColorPSO.GetPipelineStateObject() == gModelPSOs[i].GetPipelineStateObject())
        {
            return (uint8_t)i;
        }
    }

    mPSOFlags.push_back(psoFlags);

#if USE_RUNTIME_PSO
    mShaderNames.push_back(shaderNames);
#endif

    // If not found, keep the new one, and return its index
    gModelPSOs.push_back(ColorPSO);

    // The returned PSO index has read-write depth.  The index+1 tests for equal depth.
    ColorPSO.SetDepthStencilState(DepthStateTestEqual);
    ColorPSO.Finalize();

#ifdef _DEBUG
    for (uint32_t i = 0; i < gModelPSOs.size(); ++i)
        assert(ColorPSO.GetPipelineStateObject() != gModelPSOs[i].GetPipelineStateObject());
#endif
    gModelPSOs.push_back(ColorPSO);

#if USE_RUNTIME_PSO
	RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[gModelPSOs.size() - 1], shaderNames.VSName.c_str(), D3D12_SHVER_VERTEX_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[gModelPSOs.size() - 1], shaderNames.PSName.c_str(), D3D12_SHVER_PIXEL_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[gModelPSOs.size() - 2], shaderNames.VSName.c_str(), D3D12_SHVER_VERTEX_SHADER);
	RuntimePSOManager::Get().RegisterPSO(&gModelPSOs[gModelPSOs.size() - 2], shaderNames.PSName.c_str(), D3D12_SHVER_PIXEL_SHADER);
#endif

    assert(gModelPSOs.size() <= MAX_PSO);//Out of room for unique PSOs

    return (uint8_t)gModelPSOs.size() - 2;
}
