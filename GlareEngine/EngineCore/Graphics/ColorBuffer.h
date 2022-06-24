#pragma once
#include "PixelBuffer.h"
#include "Color.h"
namespace GlareEngine
{
	class ColorBuffer :
		public PixelBuffer
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

		//从交换链缓冲区创建一个颜色缓冲区。 无序访问受到限制。
		void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);

		//创建一个颜色缓冲区。如果提供了一个地址，将不会分配内存。vmem地址允许你对缓冲区进行别名。 
		void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
			DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

		//创建一个颜色缓冲区。 如果提供了一个地址，将不会分配内存。vmem地址允许你对缓冲区进行别名。 
		void CreateArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t MipMap, uint32_t ArrayCount,
			DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);


		//获取预先创建的CPU可见描述符句柄。 
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

		//这将适用于所有的纹理尺寸，但为了速度和质量，
		//建议你使用2次方的维度（但不一定是正方形）。
		//ArrayCount传0，以便在创建时为mips保留空间。 
		void GenerateMipMaps(CommandContext& Context);

	protected:

		D3D12_RESOURCE_FLAGS CombineResourceFlags(void) const
		{
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

			if (Flags == D3D12_RESOURCE_FLAG_NONE && m_FragmentCount == 1)
				Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
		}

		//计算减少到 1x1 所需的纹理层数。 这使用_BitScanReverse来寻找最高的设置位。
		//每个维度减少一半并截断比特。 维度 256 (0x100) 有 9 个 mip 级别，与维度 511 (0x1FF) 相同。 
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
		uint32_t m_NumMipMaps; // number of texture sub levels
		uint32_t m_FragmentCount;
		uint32_t m_SampleCount;
	};
}

