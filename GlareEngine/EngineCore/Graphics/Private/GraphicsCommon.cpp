#include "L3DUtil.h"
#include "GraphicsCommon.h"
#include "SamplerManager.h"
#include "CommandSignature.h"
//#include "BitonicSort.h"

using namespace GlareEngine::DirectX12Graphics;

namespace GlareEngine::DirectX12Graphics
{
	SamplerDesc SamplerLinearWrapDesc;
	SamplerDesc SamplerAnisoWrapDesc;
	SamplerDesc SamplerShadowDesc;
	SamplerDesc SamplerLinearClampDesc;
	SamplerDesc SamplerVolumeWrapDesc;
	SamplerDesc SamplerPointClampDesc;
	SamplerDesc SamplerPointBorderDesc;
	SamplerDesc SamplerLinearBorderDesc;

	D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearWrap;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerAnisoWrap;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerShadow;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearClamp;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerVolumeWrap;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerPointClamp;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerPointBorder;
	D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearBorder;

	D3D12_RASTERIZER_DESC RasterizerDefault;    // Counter-clockwise
	D3D12_RASTERIZER_DESC RasterizerDefaultMsaa;
	D3D12_RASTERIZER_DESC RasterizerDefaultCw;    // Clockwise winding
	D3D12_RASTERIZER_DESC RasterizerDefaultCwMsaa;
	D3D12_RASTERIZER_DESC RasterizerTwoSided;
	D3D12_RASTERIZER_DESC RasterizerTwoSidedMsaa;
	D3D12_RASTERIZER_DESC RasterizerShadow;
	D3D12_RASTERIZER_DESC RasterizerShadowCW;
	D3D12_RASTERIZER_DESC RasterizerShadowTwoSided;

	D3D12_BLEND_DESC BlendNoColorWrite;
	D3D12_BLEND_DESC BlendDisable;
	D3D12_BLEND_DESC BlendPreMultiplied;
	D3D12_BLEND_DESC BlendTraditional;
	D3D12_BLEND_DESC BlendAdditive;
	D3D12_BLEND_DESC BlendTraditionalAdditive;

	D3D12_DEPTH_STENCIL_DESC DepthStateDisabled;
	D3D12_DEPTH_STENCIL_DESC DepthStateReadWrite;
	D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;
	D3D12_DEPTH_STENCIL_DESC DepthStateReadOnlyReversed;
	D3D12_DEPTH_STENCIL_DESC DepthStateTestEqual;

	CommandSignature DispatchIndirectCommandSignature(1);
	CommandSignature DrawIndirectCommandSignature(1);
}


void GlareEngine::DirectX12Graphics::InitializeCommonState(void)
{

}

void GlareEngine::DirectX12Graphics::DestroyCommonState(void)
{

}
