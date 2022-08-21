#pragma once

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class SamplerDesc;
		class CommandSignature;
		class RootSignature;
		class ComputePSO;
		class GraphicsPSO;

		extern SamplerDesc SamplerLinearWrapDesc;
		extern SamplerDesc SamplerAnisoWrapDesc;
		extern SamplerDesc SamplerShadowDesc;
		extern SamplerDesc SamplerLinearClampDesc;
		extern SamplerDesc SamplerVolumeWrapDesc;
		extern SamplerDesc SamplerPointClampDesc;
		extern SamplerDesc SamplerPointBorderDesc;
		extern SamplerDesc SamplerLinearBorderDesc;

		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearWrap;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerAnisoWrap;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerShadow;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearClamp;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerVolumeWrap;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerPointClamp;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerPointBorder;
		extern D3D12_CPU_DESCRIPTOR_HANDLE SamplerLinearBorder;

		extern D3D12_RASTERIZER_DESC RasterizerDefault;
		extern D3D12_RASTERIZER_DESC RasterizerWireframe;
		extern D3D12_RASTERIZER_DESC RasterizerDefaultMsaa;
		extern D3D12_RASTERIZER_DESC RasterizerDefaultCw;
		extern D3D12_RASTERIZER_DESC RasterizerDefaultCwMsaa;
		extern D3D12_RASTERIZER_DESC RasterizerTwoSided;
		extern D3D12_RASTERIZER_DESC RasterizerTwoSidedMsaa;
		extern D3D12_RASTERIZER_DESC RasterizerShadow;
		extern D3D12_RASTERIZER_DESC RasterizerShadowCW;
		extern D3D12_RASTERIZER_DESC RasterizerShadowTwoSided;

		extern D3D12_BLEND_DESC BlendNoColorWrite;         // XXX
		extern D3D12_BLEND_DESC BlendDisable;              // 1, 0
		extern D3D12_BLEND_DESC BlendPreMultiplied;        // 1, 1-SrcA
		extern D3D12_BLEND_DESC BlendTraditional;          // SrcA, 1-SrcA
		extern D3D12_BLEND_DESC BlendAdditive;             // 1, 1
		extern D3D12_BLEND_DESC BlendTraditionalAdditive;  // SrcA, 1

		extern D3D12_DEPTH_STENCIL_DESC DepthStateDisabled;
		extern D3D12_DEPTH_STENCIL_DESC DepthStateReadWrite;
		extern D3D12_DEPTH_STENCIL_DESC DepthStateReadWriteReversed;
		extern D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;
		extern D3D12_DEPTH_STENCIL_DESC DepthStateReadOnlyReversed;
		extern D3D12_DEPTH_STENCIL_DESC DepthStateTestEqual;

		extern CommandSignature DispatchIndirectCommandSignature;
		extern CommandSignature DrawIndirectCommandSignature;

		enum eDefaultTexture
		{
			kMagenta2D,  // Useful for indicating missing textures
			kBlackOpaque2D,
			kBlackTransparent2D,
			kWhiteOpaque2D,
			kWhiteTransparent2D,
			kDefaultNormalMap,
			kBlackCubeMap,

			kNumDefaultTextures
		};
		D3D12_CPU_DESCRIPTOR_HANDLE GetDefaultTexture(eDefaultTexture texID);

		extern RootSignature g_CommonRS;

		void InitializeAllCommonState(void);
		void InitializeSampler(void);
		void InitializeRasterizer(void);
		void InitializeDepthState(void);
		void InitializeBlendState(void);

		void DestroyCommonState(void);
	}
}

