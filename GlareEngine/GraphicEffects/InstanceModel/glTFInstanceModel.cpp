#include "glTFInstanceModel.h"
#include "Render.h"

using namespace GlareEngine::Render;

void glTFInstanceModel::BuildPSO(const PSOCommonProperty CommonProperty)
{
    assert(gPSOs.size() == 0);

    // Depth Only PSOs

   /* GraphicsPSO DepthOnlyPSO(L"Render: Depth Only PSO");
    DepthOnlyPSO.SetRootSignature(gRootSignature);
    DepthOnlyPSO.SetRasterizerState(RasterizerDefault);
    DepthOnlyPSO.SetBlendState(BlendDisable);
    DepthOnlyPSO.SetDepthStencilState(DepthStateReadWrite);
    DepthOnlyPSO.SetInputLayout(_countof(posOnly), posOnly);
    DepthOnlyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    DepthOnlyPSO.SetRenderTargetFormats(0, nullptr, DSV_FORMAT);
    DepthOnlyPSO.SetVertexShader(g_pDepthOnlyVS, sizeof(g_pDepthOnlyVS));
    DepthOnlyPSO.Finalize();
    gPSOs.push_back(DepthOnlyPSO);*/
}

uint8_t glTFInstanceModel::GetPSO(uint16_t psoFlags)
{



    return uint8_t();
}
