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

			GraphicsContext& GetGraphicsContext() {
				assert(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Cannot convert async compute context to graphics");
				return reinterpret_cast<GraphicsContext&>(*this);
			}

			ComputeContext& GetComputeContext() {
				return reinterpret_cast<ComputeContext&>(*this);
			}

			ID3D12GraphicsCommandList* GetCommandList() {
				return m_CommandList;
			}

			void CopyBuffer(GPUResource& Dest, GPUResource& Src);
			void CopyBufferRegion(GPUResource& Dest, size_t DestOffset, GPUResource& Src, size_t SrcOffset, size_t NumBytes);
			void CopySubresource(GPUResource& Dest, UINT DestSubIndex, GPUResource& Src, UINT SrcSubIndex);
			void CopyCounter(GPUResource& Dest, size_t DestOffset, StructuredBuffer& Src);
			void ResetCounter(StructuredBuffer& Buf, uint32_t Value = 0);

			DynamicAlloc ReserveUploadMemory(size_t SizeInBytes)
			{
				return m_CpuLinearAllocator.Allocate(SizeInBytes);
			}

			static void InitializeTexture(GpuResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[]);
			static void InitializeBuffer(GpuResource& Dest, const void* Data, size_t NumBytes, size_t Offset = 0);
			static void InitializeTextureArraySlice(GpuResource& Dest, UINT SliceIndex, GpuResource& Src);
			static void ReadbackTexture2D(GpuResource& ReadbackBuffer, PixelBuffer& SrcBuffer);

			void WriteBuffer(GpuResource& Dest, size_t DestOffset, const void* Data, size_t NumBytes);
			void FillBuffer(GpuResource& Dest, size_t DestOffset, DWParam Value, size_t NumBytes);

			void TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
			void BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
			void InsertUAVBarrier(GpuResource& Resource, bool FlushImmediate = false);
			void InsertAliasBarrier(GpuResource& Before, GpuResource& After, bool FlushImmediate = false);
			

			void InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx);
			void ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries);
			void PIXBeginEvent(const wchar_t* label);
			void PIXEndEvent(void);
			void PIXSetMarker(const wchar_t* label);

			void SetPipelineState(const PSO& PSO);
			void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr);
			void SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[]);

			void SetPredication(ID3D12Resource* Buffer, UINT64 BufferOffset, D3D12_PREDICATION_OP Op);

		protected:

			void BindDescriptorHeaps(void);

			CommandListManager* m_OwningManager;
			ID3D12GraphicsCommandList* m_CommandList;
			ID3D12CommandAllocator* m_CurrentAllocator;

			ID3D12RootSignature* m_CurGraphicsRootSignature;
			ID3D12PipelineState* m_CurPipelineState;
			ID3D12RootSignature* m_CurComputeRootSignature;

			DynamicDescriptorHeap m_DynamicViewDescriptorHeap;        // HEAP_TYPE_CBV_SRV_UAV
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
	}
}