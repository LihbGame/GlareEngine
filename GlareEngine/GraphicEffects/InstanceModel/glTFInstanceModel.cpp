#include "glTFInstanceModel.h"
#include <stdlib.h>
#include "Graphics/Render.h"
#include "Model/Model.h"
#include "Shadow/ShadowMap.h"

using namespace GlareEngine::Render;

vector<GraphicsPSO> glTFInstanceModel::gModelPSOs;
GraphicsPSO glTFInstanceModel::sm_PBRglTFPSO;
PSOCommonProperty glTFInstanceModel::sm_PSOCommonProperty;

void glTFInstanceModel::BuildPSO(const PSOCommonProperty CommonProperty)
{
    sm_PSOCommonProperty = CommonProperty;


	//assert(gModelPSOs.size() == 0);

	//// Depth Only PSOs
	//GraphicsPSO DepthOnlyPSO(L"Render: Depth Only PSO");
	//DepthOnlyPSO.SetRootSignature(gRootSignature);

	//if (CommonProperty.IsMSAA)
	//{
	//	DepthOnlyPSO.SetRasterizerState(RasterizerDefaultMsaa);
	//	DepthOnlyPSO.SetBlendState(BlendDisableAlphaToCoverage);
	//}
	//else
	//{
	//	DepthOnlyPSO.SetRasterizerState(RasterizerDefault);
	//	DepthOnlyPSO.SetBlendState(BlendDisable);
	//}
	//DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
	//DepthOnlyPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	//DepthOnlyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, DSV_FORMAT);
	//DepthOnlyPSO.SetVertexShader(g_pDepthOnlyVS, sizeof(g_pDepthOnlyVS));
	//DepthOnlyPSO.Finalize();
	//gModelPSOs.push_back(DepthOnlyPSO);


	////Cutout Depth PSO
	//GraphicsPSO CutoutDepthPSO(L"Render: Cutout Depth PSO");
	//CutoutDepthPSO = DepthOnlyPSO;
	//CutoutDepthPSO.SetInputLayout((UINT)InputLayout::PosUV.size(), InputLayout::PosUV.data());
	//if (CommonProperty.IsMSAA)
	//{
	//	CutoutDepthPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
	//}
	//else
	//{
	//	CutoutDepthPSO.SetRasterizerState(RasterizerTwoSided);
	//}
	//CutoutDepthPSO.SetVertexShader(g_pCutoutDepthVS, sizeof(g_pCutoutDepthVS));
	//CutoutDepthPSO.SetPixelShader(g_pCutoutDepthPS, sizeof(g_pCutoutDepthPS));
	//CutoutDepthPSO.Finalize();
	//gModelPSOs.push_back(CutoutDepthPSO);


	////Skin Depth PSO
	//GraphicsPSO SkinDepthOnlyPSO(L"Render: Skin Depth PSO");
	//SkinDepthOnlyPSO = DepthOnlyPSO;
	//SkinDepthOnlyPSO.SetInputLayout((UINT)InputLayout::SkinPos.size(), InputLayout::SkinPos.data());
	//SkinDepthOnlyPSO.SetVertexShader(g_pDepthOnlySkinVS, sizeof(g_pDepthOnlySkinVS));
	//SkinDepthOnlyPSO.Finalize();
	//gModelPSOs.push_back(SkinDepthOnlyPSO);

	////Skin Cutout Depth PSO
	//GraphicsPSO SkinCutoutDepthPSO(L"Render: Skin Cutout Depth PSO");
	//SkinCutoutDepthPSO = CutoutDepthPSO;
	//SkinCutoutDepthPSO.SetInputLayout((UINT)InputLayout::SkinPosUV.size(), InputLayout::SkinPosUV.data());
	//SkinCutoutDepthPSO.SetVertexShader(g_pCutoutDepthSkinVS, sizeof(g_pCutoutDepthSkinVS));
	//SkinCutoutDepthPSO.Finalize();
	//gModelPSOs.push_back(SkinCutoutDepthPSO);

	//assert(gModelPSOs.size() == 4);

	//// Shadow PSOs

	//DepthOnlyPSO.SetRasterizerState(RasterizerShadow);
	//DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	//DepthOnlyPSO.Finalize();
	//gModelPSOs.push_back(DepthOnlyPSO);

	//CutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
	//CutoutDepthPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	//CutoutDepthPSO.Finalize();
	//gModelPSOs.push_back(CutoutDepthPSO);

	//SkinDepthOnlyPSO.SetRasterizerState(RasterizerShadow);
	//SkinDepthOnlyPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	//SkinDepthOnlyPSO.Finalize();
	//gModelPSOs.push_back(SkinDepthOnlyPSO);

	//SkinCutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
	//SkinCutoutDepthPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	//SkinCutoutDepthPSO.Finalize();
	//gModelPSOs.push_back(SkinCutoutDepthPSO);

	//assert(gModelPSOs.size() == 8);

	//// Default PSO
	//sm_PBRglTFPSO.SetRootSignature(gRootSignature);

 //   if (CommonProperty.IsMSAA)
 //   {
 //       sm_PBRglTFPSO.SetRasterizerState(RasterizerDefaultMsaa);
 //       sm_PBRglTFPSO.SetBlendState(BlendDisableAlphaToCoverage);
 //   }
 //   else
 //   {
 //       sm_PBRglTFPSO.SetRasterizerState(RasterizerDefault);
 //       sm_PBRglTFPSO.SetBlendState(BlendDisable);
 //   }
	//sm_PBRglTFPSO.SetDepthStencilState(DepthStateReadWrite);
 //   //set Input layout when you Get this PSO.
	//sm_PBRglTFPSO.SetInputLayout(0, nullptr);
	//sm_PBRglTFPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//sm_PBRglTFPSO.SetRenderTargetFormats(1, &DefaultHDRColorFormat, DSV_FORMAT, CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	//sm_PBRglTFPSO.SetVertexShader(g_pPBRVS, sizeof(g_pPBRVS));
	//sm_PBRglTFPSO.SetPixelShader(g_pPBRPS, sizeof(g_pPBRPS));

}

//uint8_t glTFInstanceModel::GetPSO(uint16_t psoFlags)
//{
//    using namespace GlareEngine::ModelPSOFlags;
//
//    GraphicsPSO ColorPSO = sm_PBRglTFPSO;
//
//    uint16_t Requirements = eHasPosition | eHasNormal;
//    assert((psoFlags & Requirements) == Requirements);
//
//    std::vector<D3D12_INPUT_ELEMENT_DESC> vertexLayout;
//    if (psoFlags & eHasPosition)
//        vertexLayout.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT });
//    
//    if (psoFlags & eHasNormal)
//        vertexLayout.push_back({ "NORMAL",   0, DXGI_FORMAT_R10G10B10A2_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT });
//    
//    if (psoFlags & eHasTangent)
//        vertexLayout.push_back({ "TANGENT",  0, DXGI_FORMAT_R10G10B10A2_UNORM,  0, D3D12_APPEND_ALIGNED_ELEMENT });
//    
//    if (psoFlags & eHasUV0)
//        vertexLayout.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT });
//    else
//        vertexLayout.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT,       1, D3D12_APPEND_ALIGNED_ELEMENT });
//    
//    if (psoFlags & eHasUV1)
//        vertexLayout.push_back({ "TEXCOORD", 1, DXGI_FORMAT_R16G16_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT });
//    
//    if (psoFlags & eHasSkin)
//    {
//        vertexLayout.push_back({ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
//        vertexLayout.push_back({ "BLENDWEIGHT", 0, DXGI_FORMAT_R16G16B16A16_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
//    }
//
//    ColorPSO.SetInputLayout((uint32_t)vertexLayout.size(), vertexLayout.data());
//
//    if (psoFlags & eHasSkin)
//    {
//        if (psoFlags & eHasTangent)
//        {
//            if (psoFlags & eHasUV1)
//            {
//                ColorPSO.SetVertexShader(g_pPBRSkinVS, sizeof(g_pPBRSkinVS));
//                ColorPSO.SetPixelShader(g_pPBRPS, sizeof(g_pPBRPS));
//            }
//            else
//            {
//                ColorPSO.SetVertexShader(g_pPBRNoUV1SkinVS, sizeof(g_pPBRNoUV1SkinVS));
//                ColorPSO.SetPixelShader(g_pPBRNoUV1PS, sizeof(g_pPBRNoUV1PS));
//            }
//        }
//        else
//        {
//            if (psoFlags & eHasUV1)
//            {
//                ColorPSO.SetVertexShader(g_pPBRNoTangentSkinVS, sizeof(g_pPBRNoTangentSkinVS));
//                ColorPSO.SetPixelShader(g_pPBRNoTangentPS, sizeof(g_pPBRNoTangentPS));
//            }
//            else
//            {
//                ColorPSO.SetVertexShader(g_pPBRNoTangentNoUV1SkinVS, sizeof(g_pPBRNoTangentNoUV1SkinVS));
//                ColorPSO.SetPixelShader(g_pPBRNoTangentNoUV1PS, sizeof(g_pPBRNoTangentNoUV1PS));
//            }
//        }
//    }
//    else
//    {
//        if (psoFlags & eHasTangent)
//        {
//            if (psoFlags & eHasUV1)
//            {
//                ColorPSO.SetVertexShader(g_pPBRDefaultVS, sizeof(g_pPBRDefaultVS));
//                ColorPSO.SetPixelShader(g_pPBRDefaultPS, sizeof(g_pPBRDefaultPS));
//            }
//            else
//            {
//                ColorPSO.SetVertexShader(g_pPBRNoUV1VS, sizeof(g_pPBRNoUV1VS));
//                ColorPSO.SetPixelShader(g_pPBRNoUV1PS, sizeof(g_pPBRNoUV1PS));
//            }
//        }
//        else
//        {
//            if (psoFlags & eHasUV1)
//            {
//                ColorPSO.SetVertexShader(g_pPBRNoTangentVS, sizeof(g_pPBRNoTangentVS));
//                ColorPSO.SetPixelShader(g_pPBRNoTangentPS, sizeof(g_pPBRNoTangentPS));
//            }
//            else
//            {
//                ColorPSO.SetVertexShader(g_pPBRNoTangentNoUV1VS, sizeof(g_pPBRNoTangentNoUV1VS));
//                ColorPSO.SetPixelShader(g_pPBRNoTangentNoUV1PS, sizeof(g_pPBRNoTangentNoUV1PS));
//            }
//        }
//    }
//
//    
//    if (psoFlags & eTwoSided)
//    {
//        if (sm_PSOCommonProperty.IsMSAA)
//        {
//            ColorPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
//            ColorPSO.SetBlendState(BlendDisableAlphaToCoverage);
//        }
//        else
//        {
//            ColorPSO.SetRasterizerState(RasterizerTwoSided);
//            ColorPSO.SetBlendState(BlendDisable);
//        }
//    }
//
//    if (psoFlags & eAlphaBlend)
//    {
//        if(sm_PSOCommonProperty.IsMSAA)
//        { 
//            ColorPSO.SetBlendState(BlendPreMultipliedAlphaToCoverage);
//        }
//        else
//        {
//            ColorPSO.SetBlendState(BlendPreMultiplied);
//        }
//        ColorPSO.SetDepthStencilState(DepthStateReadOnly);
//    }
//
//    ColorPSO.Finalize();
//
//    // Is this PSO exist?
//    for (uint32_t i = 0; i < gModelPSOs.size(); ++i)
//    {
//        if (ColorPSO.GetPipelineStateObject() == gModelPSOs[i].GetPipelineStateObject())
//        {
//            return (uint8_t)i;
//        }
//    }
//
//    // If not found, keep the new one, and return its index
//    gModelPSOs.push_back(ColorPSO);
//
//    // The returned PSO index has read-write depth.  The index+1 tests for equal depth.
//    ColorPSO.SetDepthStencilState(DepthStateTestEqual);
//    ColorPSO.Finalize();
//
//#ifdef _DEBUG
//    for (uint32_t i = 0; i < gModelPSOs.size(); ++i)
//        assert(ColorPSO.GetPipelineStateObject() != gModelPSOs[i].GetPipelineStateObject());
//#endif
//    gModelPSOs.push_back(ColorPSO);
//
//    assert(gModelPSOs.size() <= 256);//Out of room for unique PSOs
//
//    return (uint8_t)gModelPSOs.size() - 2;
//}
