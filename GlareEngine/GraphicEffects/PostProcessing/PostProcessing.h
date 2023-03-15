#pragma once
#include "Misc/RenderObject.h"

namespace PostProcessing 
{
    void Initialize(ID3D12GraphicsCommandList * CommandList);

    void BuildSRV(ID3D12GraphicsCommandList* CommandList);

    void Render(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

    void DrawBeforeToneMapping();
    void DrawAfterToneMapping();

    void DrawUI();

    void Update(float dt);

    void BuildPSO(const PSOCommonProperty CommonProperty);

    //PSO
    extern GraphicsPSO mPSO;
}

