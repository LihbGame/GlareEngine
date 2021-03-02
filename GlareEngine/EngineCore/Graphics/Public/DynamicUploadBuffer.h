#pragma once
class DynamicUploadBuffer
{
public:
	DynamicUploadBuffer() : m_GPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS_NULL), m_CPUVirtualAddress(nullptr) {}
	~DynamicUploadBuffer() { Destroy(); }

	void Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize);
	void Destroy(void);

	// Map a CPU-visible pointer to the buffer memory.  You probably don't want to leave a lot of
	// memory (100s of MB) mapped this way, so you have the option of unmapping it.
	void* Map(void);
	void Unmap(void);

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(uint32_t NumVertices, uint32_t Stride, uint32_t Offset = 0) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(uint32_t NumIndices, bool _32bit, uint32_t Offset = 0) const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuPointer(uint32_t Offset = 0) const
	{
		return m_GPUVirtualAddress + Offset;
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress;
	void* m_CPUVirtualAddress;
};

