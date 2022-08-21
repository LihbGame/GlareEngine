#include "EngineUtility.h"
#include "GraphicsCommon.h"
#include "SamplerManager.h"
#include "CommandSignature.h"
#include "ColorBuffer.h"
//#include "BitonicSort.h"

using namespace GlareEngine::DirectX12Graphics;

namespace GlareEngine
{
	namespace DirectX12Graphics
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

		ColorBuffer DefaultTextures[kNumDefaultTextures];
		D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultTexture(eDefaultTexture texID)
		{
			assert(texID < kNumDefaultTextures);
			return DefaultTextures[texID].GetSRV();
		}

		D3D12_RASTERIZER_DESC RasterizerDefault;    // Counter-clockwise
		D3D12_RASTERIZER_DESC RasterizerWireframe;
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
		D3D12_DEPTH_STENCIL_DESC DepthStateReadWriteReversed;
		D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;
		D3D12_DEPTH_STENCIL_DESC DepthStateReadOnlyReversed;
		D3D12_DEPTH_STENCIL_DESC DepthStateTestEqual;

		CommandSignature DispatchIndirectCommandSignature(1);
		CommandSignature DrawIndirectCommandSignature(1);
	}
}


void GlareEngine::DirectX12Graphics::InitializeAllCommonState(void)
{
	InitializeSampler();
	InitializeRasterizer();
	InitializeDepthState();
	InitializeBlendState();

	DefaultTextures[kMagenta2D].SetClearColor(Color(1.0f, 0.0f, 1.0f, 1.0f));
	DefaultTextures[kMagenta2D].Create(L"Magenta Default Texture", 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	DefaultTextures[kBlackOpaque2D].SetClearColor(Color(0.0f, 0.0f, 0.0f, 1.0f));
	DefaultTextures[kBlackOpaque2D].Create(L"BlackOpaque Default Texture", 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	DefaultTextures[kBlackTransparent2D].SetClearColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
	DefaultTextures[kBlackTransparent2D].Create(L"BlackTransparent Default Texture", 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	DefaultTextures[kWhiteOpaque2D].SetClearColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
	DefaultTextures[kWhiteOpaque2D].Create(L"WhiteOpaque Default Texture", 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	DefaultTextures[kWhiteTransparent2D].SetClearColor(Color(1.0f, 1.0f, 1.0f, 0.0f));
	DefaultTextures[kWhiteTransparent2D].Create(L"WhiteTransparent Default Texture", 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	DefaultTextures[kDefaultNormalMap].SetClearColor(Color(0.0f, 0.0f, 1.0f, 0.0f));
	DefaultTextures[kDefaultNormalMap].Create(L"DefaultNormalMap Default Texture", 1, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	DefaultTextures[kBlackCubeMap].SetClearColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
	DefaultTextures[kBlackCubeMap].CreateArray(L"BlackCubeMap Default Texture", 1, 1, 1, 6, DXGI_FORMAT_R16G16B16A16_FLOAT);


	DispatchIndirectCommandSignature[0].Dispatch();
	DispatchIndirectCommandSignature.Finalize();

	DrawIndirectCommandSignature[0].Draw();
	DrawIndirectCommandSignature.Finalize();
}

void GlareEngine::DirectX12Graphics::InitializeSampler(void)
{
	SamplerLinearWrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerLinearWrap = SamplerLinearWrapDesc.CreateDescriptor();

	SamplerLinearClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerLinearClampDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	SamplerLinearClamp = SamplerLinearClampDesc.CreateDescriptor();


	SamplerPointClampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	SamplerPointClampDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	SamplerPointClamp = SamplerPointClampDesc.CreateDescriptor();


	SamplerAnisoWrapDesc.MaxAnisotropy = 16;
	SamplerAnisoWrap = SamplerAnisoWrapDesc.CreateDescriptor();


	SamplerShadowDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	SamplerShadowDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	SamplerShadowDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	SamplerShadow = SamplerShadowDesc.CreateDescriptor();


	SamplerVolumeWrapDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	SamplerVolumeWrap = SamplerVolumeWrapDesc.CreateDescriptor();


	SamplerLinearBorderDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerLinearBorderDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_BORDER);
	SamplerLinearBorderDesc.SetBorderColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
	SamplerLinearBorder = SamplerLinearBorderDesc.CreateDescriptor();


	SamplerPointBorderDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	SamplerPointBorderDesc.SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE_BORDER);
	SamplerPointBorderDesc.SetBorderColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
	SamplerPointBorder = SamplerPointBorderDesc.CreateDescriptor();
}

void GlareEngine::DirectX12Graphics::InitializeRasterizer(void)
{
	// Default Rasterizer states
	RasterizerDefault.FillMode = D3D12_FILL_MODE_SOLID;
	RasterizerDefault.CullMode = D3D12_CULL_MODE_BACK;
	RasterizerDefault.FrontCounterClockwise = TRUE;
	RasterizerDefault.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	RasterizerDefault.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	RasterizerDefault.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	RasterizerDefault.DepthClipEnable = TRUE;
	RasterizerDefault.MultisampleEnable = FALSE;
	RasterizerDefault.AntialiasedLineEnable = FALSE;
	RasterizerDefault.ForcedSampleCount = 0;
	RasterizerDefault.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	RasterizerWireframe = RasterizerDefault;
	RasterizerWireframe.CullMode = D3D12_CULL_MODE_NONE;
	RasterizerWireframe.FillMode = D3D12_FILL_MODE_WIREFRAME;

	RasterizerDefaultMsaa = RasterizerDefault;
	RasterizerDefaultMsaa.MultisampleEnable = TRUE;

	RasterizerDefaultCw = RasterizerDefault;
	RasterizerDefaultCw.FrontCounterClockwise = FALSE;

	RasterizerDefaultCwMsaa = RasterizerDefaultCw;
	RasterizerDefaultCwMsaa.MultisampleEnable = TRUE;

	RasterizerTwoSided = RasterizerDefault;
	RasterizerTwoSided.CullMode = D3D12_CULL_MODE_NONE;

	RasterizerTwoSidedMsaa = RasterizerTwoSided;
	RasterizerTwoSidedMsaa.MultisampleEnable = TRUE;

	// Shadows need their own rasterizer state so we can reverse the winding of faces
	RasterizerShadow = RasterizerDefault;
	RasterizerShadow.CullMode = D3D12_CULL_MODE_FRONT;  // Hacked here rather than fixing the content
	RasterizerShadow.SlopeScaledDepthBias = 1.0f;
	RasterizerShadow.DepthBiasClamp = 0.0f;
	RasterizerShadow.DepthBias = 25000;

	RasterizerShadowTwoSided = RasterizerShadow;
	RasterizerShadowTwoSided.CullMode = D3D12_CULL_MODE_NONE;

	RasterizerShadowCW = RasterizerShadow;
	RasterizerShadowCW.FrontCounterClockwise = FALSE;
}

void GlareEngine::DirectX12Graphics::InitializeDepthState(void)
{
	DepthStateDisabled.DepthEnable = FALSE;
	DepthStateDisabled.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	DepthStateDisabled.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	DepthStateDisabled.StencilEnable = FALSE;
	DepthStateDisabled.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	DepthStateDisabled.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	DepthStateDisabled.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	DepthStateDisabled.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	DepthStateDisabled.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	DepthStateDisabled.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	DepthStateDisabled.BackFace = DepthStateDisabled.FrontFace;

	DepthStateReadWrite = DepthStateDisabled;
	DepthStateReadWrite.DepthEnable = TRUE;
	DepthStateReadWrite.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	DepthStateReadWrite.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	DepthStateReadWriteReversed = DepthStateReadWrite;
	DepthStateReadWriteReversed.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

	DepthStateReadOnly = DepthStateReadWrite;
	DepthStateReadOnly.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	DepthStateReadOnlyReversed = DepthStateReadOnly;
	DepthStateReadOnlyReversed.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	DepthStateTestEqual = DepthStateReadOnly;
	DepthStateTestEqual.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

}

void GlareEngine::DirectX12Graphics::InitializeBlendState(void)
{
	D3D12_BLEND_DESC alphaBlend = {};
	alphaBlend.IndependentBlendEnable = FALSE;
	alphaBlend.RenderTarget[0].BlendEnable = FALSE;
	alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	alphaBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	alphaBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	alphaBlend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	alphaBlend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	alphaBlend.RenderTarget[0].RenderTargetWriteMask = 0;
	BlendNoColorWrite = alphaBlend;

	alphaBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	BlendDisable = alphaBlend;

	alphaBlend.RenderTarget[0].BlendEnable = TRUE;
	BlendTraditional = alphaBlend;

	alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	BlendPreMultiplied = alphaBlend;

	alphaBlend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	BlendAdditive = alphaBlend;

	alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	BlendTraditionalAdditive = alphaBlend;
}

void GlareEngine::DirectX12Graphics::DestroyCommonState(void)
{
	for (uint32_t i = 0; i < kNumDefaultTextures; ++i)
		DefaultTextures[i].Destroy();

	DispatchIndirectCommandSignature.Destroy();
	DrawIndirectCommandSignature.Destroy();
}
