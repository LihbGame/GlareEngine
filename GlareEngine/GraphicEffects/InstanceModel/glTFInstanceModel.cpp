#include "glTFInstanceModel.h"
#include <stdlib.h>
#include "Render.h"

using namespace GlareEngine::Render;

void glTFInstanceModel::BuildPSO(const PSOCommonProperty CommonProperty)
{
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






}

uint8_t glTFInstanceModel::GetPSO(uint16_t psoFlags)
{



    return uint8_t();
}
