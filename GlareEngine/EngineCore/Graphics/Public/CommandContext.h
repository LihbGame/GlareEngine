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
#include "L3DTextureManage.h"

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		class ColorBuffer;
		class DepthBuffer;
		class L3DTextureManage;
		class GraphicsContext;
		class ComputeContext;


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


			void TransitionResource(GPUResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
			void BeginResourceTransition(GPUResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
			void InsertUAVBarrier(GPUResource& Resource, bool FlushImmediate = false);
			void InsertAliasBarrier(GPUResource& Before, GPUResource& After, bool FlushImmediate = false);


			void CopyBuffer(GPUResource& Dest, GPUResource& Src);
			void CopyBufferRegion(GPUResource& Dest, size_t DestOffset, GPUResource& Src, size_t SrcOffset, size_t NumBytes);
			void CopySubresource(GPUResource& Dest, UINT DestSubIndex, GPUResource& Src, UINT SrcSubIndex);
			void CopyCounter(GPUResource& Dest, size_t DestOffset, StructuredBuffer& Src);
			void ResetCounter(StructuredBuffer& Buf, uint32_t Value = 0);


			static void InitializeTexture(GPUResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[]);
			static void InitializeBuffer(GPUResource& Dest, const void* Data, size_t NumBytes, size_t Offset = 0);
			static void InitializeTextureArraySlice(GPUResource& Dest, UINT SliceIndex, GPUResource& Src);
			static void ReadbackTexture2D(GPUResource& ReadbackBuffer, PixelBuffer& SrcBuffer);

			void WriteBuffer(GPUResource& Dest, size_t DestOffset, const void* Data, size_t NumBytes);
			void FillBuffer(GPUResource& Dest, size_t DestOffset, DWParam Value, size_t NumBytes);

			
			void InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx);
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
				assert(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Can not convert async compute context to graphics");
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
	}
}