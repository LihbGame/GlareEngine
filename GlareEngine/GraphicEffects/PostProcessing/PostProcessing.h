#pragma once
#include "Misc/RenderObject.h"

namespace ScreenProcessing 
{
    void Initialize(ID3D12GraphicsCommandList * CommandList);

    void BuildSRV(ID3D12GraphicsCommandList* CommandList);

    void RenderFBM(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

    void Render();

    void DrawBeforeToneMapping();

    void DrawAfterToneMapping();

    void DrawUI();

    void Update(float dt, MainConstants& RenderData);

    void BuildPSO(const PSOCommonProperty CommonProperty);

    void UpsampleBlurBuffer(ComputeContext&, ColorBuffer buffer[2], const ColorBuffer& LowerResBuffer, float UpSampleBlendFactor = 0.0f);

    void BlurBuffer(ComputeContext&, ColorBuffer& SourceBuffer, ColorBuffer& TargetBuffer);

    void GaussianBlur(ComputeContext&, ColorBuffer& SourceBuffer, bool IsWideBlur = false);

    void BilateralBlur(ComputeContext&, ColorBuffer& SourceBuffer, bool IsWideBlur = false);

	void LinearizeZ(ComputeContext& Context, Camera& camera, uint32_t FrameIndex);

	void LinearizeZ(ComputeContext& Context, DepthBuffer& Depth, ColorBuffer* LinearDepth, float zMagic);

    ColorBuffer* GetLinearDepthBuffer();

    void ShutDown();

    const RootSignature& GetRootSignature();
    
}

