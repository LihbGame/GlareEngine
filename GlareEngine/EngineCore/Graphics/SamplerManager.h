#pragma once
#include "Engine/EngineUtility.h"

namespace GlareEngine
{
	class SamplerDesc : public D3D12_SAMPLER_DESC
	{
	public:
		//这些默认值与HLSL定义的根签名静态采样器的默认值匹配。 
		//因此，在此处不覆盖它们意味着您可以安全地不在HLSL中定义它们。
		SamplerDesc()
		{
			Filter = D3D12_FILTER_ANISOTROPIC;
			AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			MipLODBias = 0.0f;
			MaxAnisotropy = 16;
			ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			BorderColor[0] = 1.0f;
			BorderColor[1] = 1.0f;
			BorderColor[2] = 1.0f;
			BorderColor[3] = 1.0f;
			MinLOD = 0.0f;
			MaxLOD = D3D12_FLOAT32_MAX;
		}

		void SetTextureAddressMode(D3D12_TEXTURE_ADDRESS_MODE AddressMode)
		{
			AddressU = AddressMode;
			AddressV = AddressMode;
			AddressW = AddressMode;
		}

		void SetBorderColor(Color Border)
		{
			BorderColor[0] = Border.R();
			BorderColor[1] = Border.G();
			BorderColor[2] = Border.B();
			BorderColor[3] = Border.A();
		}

		//根据需要分配新的描述符； 尽可能返回现有描述符的句柄
		D3D12_CPU_DESCRIPTOR_HANDLE CreateDescriptor(void);


		void CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& Handle);
	};
}
