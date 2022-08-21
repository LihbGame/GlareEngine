#pragma once

#include "PixelBuffer.h"
#include "Color.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class ColorBuffer : public PixelBuffer
		{
		public:
			ColorBuffer(Color ClearColor = Color(0.0f, 0.0f, 0.0f, 0.0f))
				: m_ClearColor(ClearColor), m_NumMipMaps(0), m_FragmentCount(1), m_SampleCount(1)
			{
				m_SRVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				m_RTVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
				for (int i = 0; i < _countof(m_UAVHandle); ++i)
					m_UAVHandle[i].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
			}

			//�ӽ���������������һ����ɫ�������� ��������ܵ����ơ�
			void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);

			//����һ����ɫ������������ṩ��һ����ַ������������ڴ档vmem��ַ������Ի��������б����� 
			void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
				DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

			//����һ����ɫ�������� ����ṩ��һ����ַ������������ڴ档vmem��ַ������Ի��������б����� 
			void CreateArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t MipMap, uint32_t ArrayCount,
				DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);


			//��ȡԤ�ȴ�����CPU�ɼ������������ 
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRVHandle; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const { return m_RTVHandle; }
			const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(void) const { return m_UAVHandle[0]; }

			void SetClearColor(Color ClearColor) { m_ClearColor = ClearColor; }
			

			void SetMsaaMode(uint32_t NumColorSamples, uint32_t NumCoverageSamples)
			{
				assert(NumCoverageSamples >= NumColorSamples);
				m_FragmentCount = NumColorSamples;
				m_SampleCount = NumCoverageSamples;
			}

			Color GetClearColor(void) const { return m_ClearColor; }

		protected:

			D3D12_RESOURCE_FLAGS CombineResourceFlags(void) const
			{
				D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

				if (Flags == D3D12_RESOURCE_FLAG_NONE && m_FragmentCount == 1)
					Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
			}

			//������ٵ� 1x1 �������������� ��ʹ��_BitScanReverse��Ѱ����ߵ�����λ��
			//ÿ��ά�ȼ���һ�벢�ضϱ��ء� ά�� 256 (0x100) �� 9 �� mip ������ά�� 511 (0x1FF) ��ͬ�� 
			static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
			{
				uint32_t HighBit;
				_BitScanReverse((unsigned long*)&HighBit, Width | Height);
				return HighBit + 1;
			}

			void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);

			Color m_ClearColor;
			D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE m_UAVHandle[12];
			uint32_t m_NumMipMaps; // number of texture sublevels
			uint32_t m_FragmentCount;
			uint32_t m_SampleCount;
		};
	}
}

