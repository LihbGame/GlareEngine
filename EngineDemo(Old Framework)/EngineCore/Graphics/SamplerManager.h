#pragma once
#include "EngineUtility.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class SamplerDesc : public D3D12_SAMPLER_DESC
		{
		public:
			//��ЩĬ��ֵ��HLSL����ĸ�ǩ����̬��������Ĭ��ֵƥ�䡣 
			//��ˣ��ڴ˴�������������ζ�������԰�ȫ�ز���HLSL�ж������ǡ�
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

			//������Ҫ�����µ��������� �����ܷ��������������ľ��
			D3D12_CPU_DESCRIPTOR_HANDLE CreateDescriptor(void);


			void CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& Handle);
		};
	}
}
