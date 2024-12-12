#pragma once
#include "Color.h"
#include "GPUBuffer.h"
#include "PixelBuffer.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "DynamicDescriptorHeap.h"
#include "LinearAllocator.h"
#include "CommandListManager.h"
#include "CommandSignature.h"
#include "GraphicsCore.h"
#include "TextureManager.h"
#include "GraphicsCommon.h"

namespace GlareEngine
{
	class ColorBuffer;
	class DepthBuffer;
	class UploadBuffer;
	class TextureManage;
	class GraphicsContext;
	class ComputeContext;
	class ReadbackBuffer;

	struct DWParam
	{
		DWParam(FLOAT f) : Float(f) {}
		DWParam(UINT u) : Uint(u) {}
		DWParam(INT i) : Int(i) {}

		void operator= (FLOAT f) { Float = f; }
		void operator= (UINT u) { Uint = u; }
		void operator= (INT i) { Int = i; }

		union
		{
			FLOAT Float;
			UINT Uint;
			INT Int;
		};
	};


	//Compute Queue Resource States
#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )


	class ContextManager
	{
	public:
		ContextManager(void) {}

		CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
		void FreeContext(CommandContext*);
		void DestroyAllContexts();

	private:
		std::vector<std::unique_ptr<CommandContext> > m_ContextPool[4];
		std::queue<CommandContext*> m_AvailableContexts[4];
		std::mutex m_ContextAllocationMutex;
	};


	struct NonCopyable
	{
		NonCopyable() = default;
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	};


#pragma region CommandContext
	class CommandContext :public NonCopyable
	{
		friend ContextManager;
	private:

		CommandContext(D3D12_COMMAND_LIST_TYPE Type);

		//reset CommandContext
		void Reset(void);
	public:
		~CommandContext(void);

		//Flush Resource Barriers
		inline void FlushResourceBarriers(void);

		//通过保留命令列表和命令分配器来准备渲染 
		void Initialize(void);

		static void DestroyAllContexts(void);

		static CommandContext& Begin(const std::wstring ID = L"");

		// Flush existing commands to the GPU but keep the context alive
		uint64_t Flush(bool WaitForCompletion = false);

		// Flush existing commands and release the current context
		uint64_t Finish(bool WaitForCompletion = false);
		// Flush current commands and release the PreFrame context
		uint64_t Finish(uint64_t PreFenceValue, bool WaitForCompletion = false);

		void TransitionResource(GPUResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
		void BeginResourceTransition(GPUResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
		void InsertUAVBarrier(GPUResource& Resource, bool FlushImmediate = false);
		void InsertAliasBarrier(GPUResource& Before, GPUResource& After, bool FlushImmediate = false);


		void CopyBuffer(GPUResource& Dest, GPUResource& Src);
		void CopyBufferRegion(GPUResource& Dest, size_t DestOffset, GPUResource& Src, size_t SrcOffset, size_t NumBytes);
		void CopyTextureRegion(GPUResource& Dest, UINT x, UINT y, UINT z, GPUResource& Source, RECT& rect);
		void CopySubresource(GPUResource& Dest, UINT DestSubIndex, GPUResource& Src, UINT SrcSubIndex);
		void CopyCounter(GPUResource& Dest, size_t DestOffset, StructuredBuffer& Src);
		void ResetCounter(StructuredBuffer& Buf, uint32_t Value = 0);


		static void InitializeTexture(GPUResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[]);
		static void InitializeBuffer(GPUBuffer& Dest, const void* Data, size_t NumBytes, size_t Offset = 0);
		static void InitializeBuffer(GPUBuffer& Dest, const UploadBuffer& Src, size_t SrcOffset, size_t NumBytes = -1, size_t DestOffset = 0);
		static void InitializeTextureArraySlice(GPUResource& Dest, UINT SliceIndex, GPUResource& Src);
		uint32_t ReadbackTexture(ReadbackBuffer& ReadbackBuffer, PixelBuffer& SrcBuffer);

		void WriteBuffer(GPUResource& Dest, size_t DestOffset, const void* Data, size_t NumBytes);
		void FillBuffer(GPUResource& Dest, size_t DestOffset, DWParam Value, size_t NumBytes);


		void InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx);
		void BeginTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx);
		void ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries);

		void PIXBeginEvent(const wchar_t* label);
		void PIXEndEvent(void);
		void PIXSetMarker(const wchar_t* label);


		void SetPipelineState(const PSO& PSO);
		void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr);
		void SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[]);


		void SetPredication(ID3D12Resource* Buffer, UINT64 BufferOffset, D3D12_PREDICATION_OP Op);


		//Get Graphics Context
		GraphicsContext& GetGraphicsContext()
		{
			assert(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE);
			return reinterpret_cast<GraphicsContext&>(*this);
		}

		//Get Compute Context
		ComputeContext& GetComputeContext()
		{
			return reinterpret_cast<ComputeContext&>(*this);
		}

		//Get Command List
		ID3D12GraphicsCommandList* GetCommandList()
		{
			return m_CommandList;
		}

		//Reserve Upload Memory
		DynamicAlloc ReserveUploadMemory(size_t SizeInBytes)
		{
			return m_CPULinearAllocator.Allocate(SizeInBytes);
		}

	protected:

		void BindDescriptorHeaps(void);

		CommandListManager* m_CommandListManager;
		ID3D12GraphicsCommandList* m_CommandList;
		ID3D12CommandAllocator* m_CurrentAllocator;

		ID3D12RootSignature* m_CurrentGraphicsRootSignature;
		ID3D12RootSignature* m_CurrentComputeRootSignature;
		ID3D12PipelineState* m_CurrentPipelineState;

		DynamicDescriptorHeap m_DynamicViewDescriptorHeap;       // HEAP_TYPE_CBV_SRV_UAV
		DynamicDescriptorHeap m_DynamicSamplerDescriptorHeap;    // HEAP_TYPE_SAMPLER

		D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
		UINT m_NumBarriersToFlush;

		ID3D12DescriptorHeap* m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

		LinearAllocator m_CPULinearAllocator;
		LinearAllocator m_GPULinearAllocator;

		std::wstring m_ID;
		void SetID(const std::wstring& ID) { m_ID = ID; }

		D3D12_COMMAND_LIST_TYPE m_Type;
	};
#pragma endregion

#pragma region GraphicsContext
	class GraphicsContext : public CommandContext
	{
	public:

		static GraphicsContext& Begin(const std::wstring& ID = L"")
		{
			GraphicsContext& Context = CommandContext::Begin(ID).GetGraphicsContext();
			return Context;
		}

		void ClearUAV(GPUBuffer& Target);
		void ClearUAV(ColorBuffer& Target);
		void ClearRenderTarget(ColorBuffer& Target);
		void ClearDepth(DepthBuffer& Target);
		void ClearDepth(DepthBuffer& Target, float ClearDepth);
		void ClearStencil(DepthBuffer& Target);
		void ClearDepthAndStencil(DepthBuffer& Target, float ClearDepth);
		void ClearColor(ColorBuffer& Target, D3D12_RECT* Rect = nullptr);
		void ClearColor(ColorBuffer& Target, float Colour[4], D3D12_RECT* Rect = nullptr);


		void SetRootSignature(const RootSignature& RootSig);

		void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[]);
		void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
		void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets(1, &RTV); }
		void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(1, &RTV, DSV); }
		void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(0, nullptr, DSV); }

		void SetViewport(const D3D12_VIEWPORT& vp);
		void SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f);
		void SetScissor(const D3D12_RECT& rect);
		void SetScissor(UINT left, UINT top, UINT right, UINT bottom);
		void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
		void SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h);
		void SetStencilRef(UINT StencilRef);
		void SetBlendFactor(Color BlendFactor);
		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);

		void SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants);
		void SetConstant(UINT RootIndex, DWParam Val, UINT Offset = 0);
		void SetConstants(UINT RootIndex, DWParam X);
		void SetConstants(UINT RootIndex, DWParam X, DWParam Y);
		void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z);
		void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W);
		void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);
		void SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData);
		void SetBufferSRV(UINT RootIndex, const GPUBuffer& SRV, UINT64 Offset = 0);
		void SetBufferUAV(UINT RootIndex, const GPUBuffer& UAV, UINT64 Offset = 0);
		void SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle);

		void SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
		void SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
		void SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
		void SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);

		void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
		void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
		void SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[]);
		void SetDynamicVB(UINT Slot, size_t NumVertices, size_t VertexStride, const void* VBData);
		void SetDynamicIB(size_t IndexCount, const uint16_t* IBData);
		void SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData);

		void Draw(UINT VertexCount, UINT VertexStartOffset = 0);
		void DrawIndexed(UINT IndexCount, UINT StartIndexLocation = 0, INT BaseVertexLocation = 0);
		void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
			UINT StartVertexLocation = 0, UINT StartInstanceLocation = 0);
		void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
			INT BaseVertexLocation, UINT StartInstanceLocation);
		void DrawIndirect(GPUBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset = 0);
		void ExecuteIndirect(CommandSignature& CommandSig, GPUBuffer& ArgumentBuffer, uint64_t ArgumentStartOffset = 0,
			uint32_t MaxCommands = 1, GPUBuffer* CommandCounterBuffer = nullptr, uint64_t CounterOffset = 0);

		void BeginQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex);
		void EndQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex);
		void ResolveQueryData(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource* DestinationBuffer, UINT64 DestinationBufferOffset);

	private:
	};
#pragma endregion

#pragma region ComputeContext
	class ComputeContext : public CommandContext
	{
	public:

		static ComputeContext& Begin(const std::wstring& ID = L"", bool Async = false);

		void ClearUAV(GPUBuffer& Target);
		void ClearUAV(ColorBuffer& Target);

		void SetRootSignature(const RootSignature& RootSig);

		void SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants);
		void SetConstant(UINT RootIndex, DWParam Val, UINT Offset = 0);
		void SetConstants(UINT RootIndex, DWParam X);
		void SetConstants(UINT RootIndex, DWParam X, DWParam Y);
		void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z);
		void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W);
		void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);
		void SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData);
		void SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData);
		void SetBufferSRV(UINT RootIndex, const GPUBuffer& SRV, UINT64 Offset = 0);
		void SetBufferUAV(UINT RootIndex, const GPUBuffer& UAV, UINT64 Offset = 0);
		void SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle);

		void SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
		void SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
		void SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
		void SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);

		void Dispatch(size_t GroupCountX = 1, size_t GroupCountY = 1, size_t GroupCountZ = 1);
		void Dispatch1D(size_t ThreadCountX, size_t GroupSizeX = 64);
		void Dispatch2D(size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX = 8, size_t GroupSizeY = 8);
		void Dispatch3D(size_t ThreadCountX, size_t ThreadCountY, size_t ThreadCountZ, size_t GroupSizeX, size_t GroupSizeY, size_t GroupSizeZ);
		void DispatchIndirect(GPUBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset = 0);
		void ExecuteIndirect(CommandSignature& CommandSig, GPUBuffer& ArgumentBuffer, uint64_t ArgumentStartOffset = 0,
			uint32_t MaxCommands = 1, GPUBuffer* CommandCounterBuffer = nullptr, uint64_t CounterOffset = 0);

	private:
	};
#pragma endregion


	inline void CommandContext::FlushResourceBarriers(void)
	{
		if (m_NumBarriersToFlush > 0)
		{
			m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
			m_NumBarriersToFlush = 0;
		}
	}


	inline	void CommandContext::CopyCounter(GPUResource& Dest, size_t DestOffset, StructuredBuffer& Src)
	{
		TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
		TransitionResource(Src.GetCounterBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		FlushResourceBarriers();
		m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetCounterBuffer().GetResource(), 0, 4);
	}

	inline	void CommandContext::ResetCounter(StructuredBuffer& Buf, uint32_t Value)
	{
		FillBuffer(Buf.GetCounterBuffer(), 0, Value, sizeof(uint32_t));
		TransitionResource(Buf.GetCounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}


	inline void CommandContext::CopyBuffer(GPUResource& Dest, GPUResource& Src)
	{
		TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
		TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
		FlushResourceBarriers();
		m_CommandList->CopyResource(Dest.GetResource(), Src.GetResource());
	}

	inline void CommandContext::CopyBufferRegion(GPUResource& Dest, size_t DestOffset, GPUResource& Src, size_t SrcOffset, size_t NumBytes)
	{
		TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
		//TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
		FlushResourceBarriers();
		m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetResource(), SrcOffset, NumBytes);
	}

	inline void CommandContext::InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx)
	{
		m_CommandList->EndQuery(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QueryIdx);
	}

	inline void CommandContext::BeginTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx)
	{
		m_CommandList->BeginQuery(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QueryIdx);
	}

	inline void CommandContext::ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries)
	{
		m_CommandList->ResolveQueryData(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, NumQueries, pReadbackHeap, 0);
	}

	inline void CommandContext::SetPipelineState(const PSO& PSO)
	{
		ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
		if (PipelineState == m_CurrentPipelineState)
			return;

		m_CommandList->SetPipelineState(PipelineState);
		m_CurrentPipelineState = PipelineState;
	}

	inline void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr)
	{
		if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
		{
			m_CurrentDescriptorHeaps[Type] = HeapPtr;
			BindDescriptorHeaps();
		}
	}


	inline void CommandContext::SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[])
	{
		bool AnyChanged = false;

		for (UINT i = 0; i < HeapCount; ++i)
		{
			if (m_CurrentDescriptorHeaps[Type[i]] != HeapPtrs[i])
			{
				m_CurrentDescriptorHeaps[Type[i]] = HeapPtrs[i];
				AnyChanged = true;
			}
		}
		if (AnyChanged)
			BindDescriptorHeaps();
	}

	inline void CommandContext::SetPredication(ID3D12Resource* Buffer, UINT64 BufferOffset, D3D12_PREDICATION_OP Op)
	{
		m_CommandList->SetPredication(Buffer, BufferOffset, Op);
	}


	inline void GraphicsContext::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
	{
		SetViewport((float)x, (float)y, (float)w, (float)h);
		SetScissor(x, y, x + w, y + h);
	}

	inline void GraphicsContext::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
	{
		SetScissor(CD3DX12_RECT(left, top, right, bottom));
	}

	inline void GraphicsContext::SetStencilRef(UINT ref)
	{
		m_CommandList->OMSetStencilRef(ref);
	}

	inline void GraphicsContext::SetBlendFactor(Color BlendFactor)
	{
		m_CommandList->OMSetBlendFactor(BlendFactor.GetPtr());
	}

	inline void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
	{
		m_CommandList->IASetPrimitiveTopology(Topology);
	}

	inline void GraphicsContext::SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants)
	{
		m_CommandList->SetGraphicsRoot32BitConstants(RootIndex, NumConstants, pConstants, 0);
	}

	inline void GraphicsContext::SetConstant(UINT RootIndex, DWParam Val, UINT Offset)
	{
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Val.Uint, Offset);
	}

	inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X)
	{
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
	}

	inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X, DWParam Y)
	{
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Y.Uint, 1);
	}

	inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z)
	{
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Y.Uint, 1);
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Z.Uint, 2);
	}

	inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W)
	{
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Y.Uint, 1);
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Z.Uint, 2);
		m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, W.Uint, 3);
	}

	inline void GraphicsContext::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV)
	{
		m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, CBV);
	}

	inline void GraphicsContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData)
	{
		assert(BufferData != nullptr && Math::IsAligned(BufferSize, 16));
		DynamicAlloc cb = m_CPULinearAllocator.Allocate(BufferSize);
		memcpy(cb.DataPtr, BufferData, BufferSize);
		m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, cb.GPUAddress);
	}

	inline void GraphicsContext::SetBufferSRV(UINT RootIndex, const GPUBuffer& SRV, UINT64 Offset)
	{
		assert((SRV.m_UsageState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) != 0);
		m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, SRV.GetGPUVirtualAddress() + Offset);
	}

	inline void GraphicsContext::SetBufferUAV(UINT RootIndex, const GPUBuffer& UAV, UINT64 Offset)
	{
		assert((UAV.m_UsageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
		m_CommandList->SetGraphicsRootUnorderedAccessView(RootIndex, UAV.GetGPUVirtualAddress() + Offset);
	}

	inline void GraphicsContext::SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle)
	{
		m_CommandList->SetGraphicsRootDescriptorTable(RootIndex, FirstHandle);
	}

	inline void GraphicsContext::SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
	{
		SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
	}

	inline void GraphicsContext::SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		m_DynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
	}

	inline void GraphicsContext::SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
	{
		SetDynamicSamplers(RootIndex, Offset, 1, &Handle);
	}

	inline void GraphicsContext::SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		m_DynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
	}

	inline void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView)
	{
		m_CommandList->IASetIndexBuffer(&IBView);
	}

	inline void GraphicsContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView)
	{
		SetVertexBuffers(Slot, 1, &VBView);
	}

	inline void GraphicsContext::SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[])
	{
		m_CommandList->IASetVertexBuffers(StartSlot, Count, VBViews);
	}

	inline void GraphicsContext::SetDynamicVB(UINT Slot, size_t NumVertices, size_t VertexStride, const void* VBData)
	{
		assert(VBData != nullptr && Math::IsAligned(VBData, 16));

		size_t BufferSize = Math::AlignUp(NumVertices * VertexStride, 16);
		DynamicAlloc vb = m_CPULinearAllocator.Allocate(BufferSize);

		SIMDMemoryCopy(vb.DataPtr, VBData, BufferSize >> 4);

		D3D12_VERTEX_BUFFER_VIEW VBView;
		VBView.BufferLocation = vb.GPUAddress;
		VBView.SizeInBytes = (UINT)BufferSize;
		VBView.StrideInBytes = (UINT)VertexStride;

		m_CommandList->IASetVertexBuffers(Slot, 1, &VBView);
	}

	inline void GraphicsContext::SetDynamicIB(size_t IndexCount, const uint16_t* IBData)
	{
		assert(IBData != nullptr && Math::IsAligned(IBData, 16));

		size_t BufferSize = Math::AlignUp(IndexCount * sizeof(uint16_t), 16);
		DynamicAlloc ib = m_CPULinearAllocator.Allocate(BufferSize);

		SIMDMemoryCopy(ib.DataPtr, IBData, BufferSize >> 4);

		D3D12_INDEX_BUFFER_VIEW IBView;
		IBView.BufferLocation = ib.GPUAddress;
		IBView.SizeInBytes = (UINT)(IndexCount * sizeof(uint16_t));
		IBView.Format = DXGI_FORMAT_R16_UINT;

		m_CommandList->IASetIndexBuffer(&IBView);
	}

	inline void GraphicsContext::SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData)
	{
		assert(BufferData != nullptr && Math::IsAligned(BufferData, 16));
		DynamicAlloc cb = m_CPULinearAllocator.Allocate(BufferSize);
		memcpy(cb.DataPtr, BufferData, BufferSize);
		m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, cb.GPUAddress);
	}

	inline void GraphicsContext::Draw(UINT VertexCount, UINT VertexStartOffset)
	{
		DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
	}

	inline void GraphicsContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
	{
		DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
	}

	inline void GraphicsContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
	{
		FlushResourceBarriers();
		m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
		m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
		m_CommandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	}

	inline void GraphicsContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
	{
		FlushResourceBarriers();
		m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
		m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
		m_CommandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
	}

	inline void GraphicsContext::DrawIndirect(GPUBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset)
	{
		ExecuteIndirect(GlareEngine::DrawIndirectCommandSignature, ArgumentBuffer, ArgumentBufferOffset);
	}

	inline void GraphicsContext::ExecuteIndirect(CommandSignature& CommandSig, GPUBuffer& ArgumentBuffer, uint64_t ArgumentStartOffset, uint32_t MaxCommands, GPUBuffer* CommandCounterBuffer, uint64_t CounterOffset)
	{
		FlushResourceBarriers();
		m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
		m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
		m_CommandList->ExecuteIndirect(CommandSig.GetSignature(), MaxCommands,
			ArgumentBuffer.GetResource(), ArgumentStartOffset,
			CommandCounterBuffer == nullptr ? nullptr : CommandCounterBuffer->GetResource(), CounterOffset);
	}
}