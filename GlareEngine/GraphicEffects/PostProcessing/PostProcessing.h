#pragma once
#include "Misc/RenderObject.h"

namespace PostProcessing 
{
    void Initialize(ID3D12GraphicsCommandList * CommandList);

    void BuildSRV(ID3D12GraphicsCommandList* CommandList);

    void RenderFBM(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

    void Render(GraphicsContext& Context);

    void DrawBeforeToneMapping();
    void DrawAfterToneMapping();

    void DrawUI();

    void Update(float dt);

    void BuildPSO(const PSOCommonProperty CommonProperty);

    void UpsampleBlurBuffer(ComputeContext&, ColorBuffer buffer[2], const ColorBuffer& LowerResBuffer, float UpSampleBlendFactor = 0.0f);

    void BlurBuffer(ComputeContext&, ColorBuffer SourceBuffer, ColorBuffer TargetBuffer);

    void ShutDown();
    //PSO
    extern GraphicsPSO mPSO;
}

