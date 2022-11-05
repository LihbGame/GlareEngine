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

using namespace GlareEngine::Render;

vector<GraphicsPSO> glTFInstanceModel::gModelPSOs;
GraphicsPSO glTFInstanceModel::sm_PBRglTFPSO;
PSOCommonProperty glTFInstanceModel::sm_PSOCommonProperty;

void glTFInstanceModel::BuildPSO(const PSOCommonProperty CommonProperty)
{
    sm_PSOCommonProperty = CommonProperty;

	gModelPSOs.clear();
	//assert(gModelPSOs.size() == 0);

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
		DepthOnlyPSO.SetBlendState(BlendDisable);
	}
	DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
	DepthOnlyPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	DepthOnlyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, DSV_FORMAT);
	DepthOnlyPSO.SetVertexShader(g_pDepthOnlyVS, sizeof(g_pDepthOnlyVS));
	DepthOnlyPSO.Finalize();
	gModelPSOs.push_back(DepthOnlyPSO);


	//Cutout Depth PSO
	GraphicsPSO CutoutDepthPSO(L"Render: Cutout Depth PSO");
	CutoutDepthPSO = DepthOnlyPSO;
	CutoutDepthPSO.SetInputLayout((UINT)InputLayout::PosUV.size(), InputLayout::PosUV.data());
	if (CommonProperty.IsMSAA)
	{
		CutoutDepthPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
	}
	else
	{
		CutoutDepthPSO.SetRasterizerState(RasterizerTwoSided);
	}
	CutoutDepthPSO.SetVertexShader(g_pCutoutDepthVS, sizeof(g_pCutoutDepthVS));
	CutoutDepthPSO.SetPixelShader(g_pCutoutDepthPS, sizeof(g_pCutoutDepthPS));
	CutoutDepthPSO.Finalize();
	gModelPSOs.push_back(CutoutDepthPSO);


	//Skin Depth PSO
	GraphicsPSO SkinDepthOnlyPSO(L"Render: Skin Depth PSO");
	SkinDepthOnlyPSO = DepthOnlyPSO;
	SkinDepthOnlyPSO.SetInputLayout((UINT)InputLayout::SkinPos.size(), InputLayout::SkinPos.data());
	SkinDepthOnlyPSO.SetVertexShader(g_pDepthOnlySkinVS, sizeof(g_pDepthOnlySkinVS));
	SkinDepthOnlyPSO.Finalize();
	gModelPSOs.push_back(SkinDepthOnlyPSO);

	//Skin Cutout Depth PSO
	GraphicsPSO SkinCutoutDepthPSO(L"Render: Skin Cutout Depth PSO");
	SkinCutoutDepthPSO = CutoutDepthPSO;
	SkinCutoutDepthPSO.SetInputLayout((UINT)InputLayout::SkinPosUV.size(), InputLayout::SkinPosUV.data());
	SkinCutoutDepthPSO.SetVertexShader(g_pCutoutDepthSkinVS, sizeof(g_pCutoutDepthSkinVS));
	SkinCutoutDepthPSO.Finalize();
	gModelPSOs.push_back(SkinCutoutDepthPSO);

	assert(gModelPSOs.size() == 4);

	// Shadow PSOs
	DepthOnlyPSO.SetRasterizerState(RasterizerShadow);
	DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	DepthOnlyPSO.Finalize();
	gModelPSOs.push_back(DepthOnlyPSO);

	CutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
	CutoutDepthPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	CutoutDepthPSO.Finalize();
	gModelPSOs.push_back(CutoutDepthPSO);

	SkinDepthOnlyPSO.SetRasterizerState(RasterizerShadow);
	SkinDepthOnlyPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	SkinDepthOnlyPSO.Finalize();
	gModelPSOs.push_back(SkinDepthOnlyPSO);

	SkinCutoutDepthPSO.SetRasterizerState(RasterizerShadowTwoSided);
	SkinCutoutDepthPSO.SetRenderTargetFormats(0, nullptr, ShadowMap::mFormat);
	SkinCutoutDepthPSO.Finalize();
	gModelPSOs.push_back(SkinCutoutDepthPSO);

	assert(gModelPSOs.size() == 8);

	// Default PSO
	sm_PBRglTFPSO.SetRootSignature(gRootSignature);

    if (CommonProperty.IsMSAA)
    {
        sm_PBRglTFPSO.SetRasterizerState(RasterizerDefaultMsaa);
        sm_PBRglTFPSO.SetBlendState(BlendDisableAlphaToCoverage);
    }
    else
    {
        sm_PBRglTFPSO.SetRasterizerState(RasterizerDefault);
        sm_PBRglTFPSO.SetBlendState(BlendDisable);
    }
	sm_PBRglTFPSO.SetDepthStencilState(DepthStateReadWrite);
    //set Input layout when you get this PSO.
	sm_PBRglTFPSO.SetInputLayout(0, nullptr);
	sm_PBRglTFPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	sm_PBRglTFPSO.SetRenderTargetFormats(1, &DefaultHDRColorFormat, DSV_FORMAT, CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	sm_PBRglTFPSO.SetVertexShader(g_pModelVS, sizeof(g_pModelVS));
	sm_PBRglTFPSO.SetPixelShader(g_pModelPS, sizeof(g_pModelPS));

}

uint8_t glTFInstanceModel::GetPSO(uint16_t psoFlags)
{
    using namespace GlareEngine::ModelPSOFlags;

    GraphicsPSO ColorPSO = sm_PBRglTFPSO;

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
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoUV1SkinVS, sizeof(g_pModelNoUV1SkinVS));
                ColorPSO.SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));
            }
        }
        else
        {
            if (psoFlags & eHasUV1)
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentSkinVS, sizeof(g_pModelNoTangentSkinVS));
                ColorPSO.SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentNoUV1SkinVS, sizeof(g_pModelNoTangentNoUV1SkinVS));
                ColorPSO.SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));
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
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoUV1VS, sizeof(g_pModelNoUV1VS));
                ColorPSO.SetPixelShader(g_pModelNoUV1PS, sizeof(g_pModelNoUV1PS));
            }
        }
        else
        {
            if (psoFlags & eHasUV1)
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentVS, sizeof(g_pModelNoTangentVS));
                ColorPSO.SetPixelShader(g_pModelNoTangentPS, sizeof(g_pModelNoTangentPS));
            }
            else
            {
                ColorPSO.SetVertexShader(g_pModelNoTangentNoUV1VS, sizeof(g_pModelNoTangentNoUV1VS));
                ColorPSO.SetPixelShader(g_pModelNoTangentNoUV1PS, sizeof(g_pModelNoTangentNoUV1PS));
            }
        }
    }

    
    if (psoFlags & eTwoSided)
    {
        if (sm_PSOCommonProperty.IsMSAA)
        {
            ColorPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
            ColorPSO.SetBlendState(BlendDisableAlphaToCoverage);
        }
        else
        {
            ColorPSO.SetRasterizerState(RasterizerTwoSided);
            ColorPSO.SetBlendState(BlendDisable);
        }
    }

    if (psoFlags & eAlphaBlend)
    {
        if(sm_PSOCommonProperty.IsMSAA)
        { 
            ColorPSO.SetBlendState(BlendPreMultipliedAlphaToCoverage);
        }
        else
        {
            ColorPSO.SetBlendState(BlendPreMultiplied);
        }
        ColorPSO.SetDepthStencilState(DepthStateReadOnly);
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

    assert(gModelPSOs.size() <= 256);//Out of room for unique PSOs

    return (uint8_t)gModelPSOs.size() - 2;
}
