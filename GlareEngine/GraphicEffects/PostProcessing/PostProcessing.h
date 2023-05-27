#pragma once
#include "Misc/RenderObject.h"

namespace ScreenProcessing 
{
    void Initialize(ID3D12GraphicsCommandList * CommandList);

    void BuildSRV(ID3D12GraphicsCommandList* CommandList);

    void RenderFBM(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

    void Render(MainConstants& RenderData);

    void DrawBeforeToneMapping();

    void DrawAfterToneMapping();

    void DrawUI();

    void Update(float dt);

    void BuildPSO(const PSOCommonProperty CommonProperty);

    void UpsampleBlurBuffer(ComputeContext&, ColorBuffer buffer[2], const ColorBuffer& LowerResBuffer, float UpSampleBlendFactor = 0.0f);

    void BlurBuffer(ComputeContext&, ColorBuffer& SourceBuffer, ColorBuffer& TargetBuffer);

    void GaussianBlur(ComputeContext&, ColorBuffer& SourceBuffer, ColorBuffer& TargetBuffer, bool IsWideBlur = false);

    void BilateralBlur(ComputeContext&, ColorBuffer& SourceBuffer, ColorBuffer& TargetBuffer, bool IsWideBlur = false);

    void ShutDown();

    const RootSignature& GetRootSignature();
    
}

