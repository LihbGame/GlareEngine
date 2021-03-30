#include "L3DUtil.h"
#include "CommandContext.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"

namespace GlareEngine 
{
    namespace DirectX12Graphics
    {


#pragma region ContextManager
		CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
		{
			std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
			auto& AvailableContexts = m_AvailableContexts[Type];

			CommandContext* ret = nullptr;
			if (AvailableContexts.empty())
			{
				ret = new CommandContext(Type);
				m_ContextPool[Type].emplace_back(ret);
				ret->Initialize();
			}
			else
			{
				ret = AvailableContexts.front();
				AvailableContexts.pop();
				ret->Reset();
			}
			assert(ret != nullptr);
			assert(ret->m_Type == Type);
			return ret;
		}

		void ContextManager::FreeContext(CommandContext* Context)
		{
			assert(Context != nullptr);
			std::lock_guard<std::mutex> LockGuard(m_ContextAllocationMutex);
			m_AvailableContexts[Context->m_Type].push(Context);
		}

		void ContextManager::DestroyAllContexts(void)
		{
			for (uint32_t i = 0; i < 4; ++i)
				m_ContextPool[i].clear();
		}
#pragma endregion

#pragma region CommandContext
		CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type) :
			m_Type(Type),
			m_DynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
			m_DynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
			m_CPULinearAllocator(CPUWritable),
			m_GPULinearAllocator(GPUExclusive)
		{
			m_OwningManager = nullptr;
			m_CommandList = nullptr;
			m_CurrentAllocator = nullptr;
			ZeroMemory(m_CurrentDescriptorHeaps, sizeof(m_CurrentDescriptorHeaps));

			m_CurGraphicsRootSignature = nullptr;
			m_CurPipelineState = nullptr;
			m_CurComputeRootSignature = nullptr;
			m_NumBarriersToFlush = 0;
		}

		CommandContext::~CommandContext(void)
		{
			if (m_CommandList != nullptr)
				m_CommandList->Release();
		}

		inline void CommandContext::FlushResourceBarriers(void)
		{
			if (m_NumBarriersToFlush > 0)
			{
				m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
				m_NumBarriersToFlush = 0;
			}
		}

		void CommandContext::Initialize(void)
		{
			g_CommandManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
		}


		void CommandContext::BindDescriptorHeaps(void)
		{
			UINT NumDescriptorHeaps = 0;
			ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
			for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
			{
				ID3D12DescriptorHeap* HeapIter = m_CurrentDescriptorHeaps[i];
				if (HeapIter != nullptr)
					HeapsToBind[NumDescriptorHeaps++] = HeapIter;
			}

			if (NumDescriptorHeaps > 0)
				m_CommandList->SetDescriptorHeaps(NumDescriptorHeaps, HeapsToBind);
		}

		void CommandContext::Reset(void)
		{
			//我们只对先前释放的上下文调用Reset()。 命令列表会持续存在，但我们必须请求一个新的分配器。 
			assert(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
			m_CurrentAllocator = g_CommandManager.GetQueue(m_Type).RequestAllocator();
			m_CommandList->Reset(m_CurrentAllocator, nullptr);

			m_CurGraphicsRootSignature = nullptr;
			m_CurPipelineState = nullptr;
			m_CurComputeRootSignature = nullptr;
			m_NumBarriersToFlush = 0;

			BindDescriptorHeaps();
		}

		void CommandContext::DestroyAllContexts(void)
		{
			LinearAllocator::DestroyAll();
			DynamicDescriptorHeap::DestroyAll();
			g_ContextManager.DestroyAllContexts();
		}

		CommandContext& CommandContext::Begin(const std::wstring ID)
		{
			CommandContext* NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
			NewContext->SetID(ID);
			return *NewContext;
		}

		uint64_t CommandContext::Flush(bool WaitForCompletion)
		{
			FlushResourceBarriers();

			assert(m_CurrentAllocator != nullptr);

			uint64_t FenceValue = g_CommandManager.GetQueue(m_Type).ExecuteCommandList(m_CommandList);

			if (WaitForCompletion)
				g_CommandManager.WaitForFence(FenceValue);

			//
			// Reset the command list and restore previous state
			//

			m_CommandList->Reset(m_CurrentAllocator, nullptr);

			if (m_CurGraphicsRootSignature)
			{
				m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature);
			}
			if (m_CurComputeRootSignature)
			{
				m_CommandList->SetComputeRootSignature(m_CurComputeRootSignature);
			}
			if (m_CurPipelineState)
			{
				m_CommandList->SetPipelineState(m_CurPipelineState);
			}

			BindDescriptorHeaps();

			return FenceValue;
		}


		uint64_t CommandContext::Finish(bool WaitForCompletion)
		{
			assert(m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT || m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

			FlushResourceBarriers();

			assert(m_CurrentAllocator != nullptr);

			CommandQueue& Queue = g_CommandManager.GetQueue(m_Type);

			uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
			Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
			m_CurrentAllocator = nullptr;

			m_CPULinearAllocator.CleanupUsedPages(FenceValue);
			m_GPULinearAllocator.CleanupUsedPages(FenceValue);
			m_DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue);
			m_DynamicSamplerDescriptorHeap.CleanupUsedHeaps(FenceValue);

			if (WaitForCompletion)
				g_CommandManager.WaitForFence(FenceValue);

			g_ContextManager.FreeContext(this);

			return FenceValue;
		}

		void CommandContext::TransitionResource(GPUResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
		{
			D3D12_RESOURCE_STATES OldState = Resource.m_UsageState;

			if (m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
			{
				assert((OldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == OldState,"This is not Compute States!");
				assert((NewState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == NewState,"This is not Compute States!");
			}

			if (OldState != NewState)
			{
				assert(m_NumBarriersToFlush < 16, "The arbitrary limit of the buffer barrier is exceeded.");
				D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

				BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				BarrierDesc.Transition.pResource = Resource.GetResource();
				BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				BarrierDesc.Transition.StateBefore = OldState;
				BarrierDesc.Transition.StateAfter = NewState;

				//Check if we have started the transition. (多线程同步)
				if (NewState == Resource.m_TransitioningState)
				{
					BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
					Resource.m_TransitioningState = (D3D12_RESOURCE_STATES)-1;
				}
				else
				{
					BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				}
				Resource.m_UsageState = NewState;
			}
			else if (NewState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				InsertUAVBarrier(Resource, FlushImmediate);

			if (FlushImmediate || m_NumBarriersToFlush == 16)
				FlushResourceBarriers();
		}

		void CommandContext::BeginResourceTransition(GPUResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
		{
			// If it's already transitioning, finish that transition
			if (Resource.m_TransitioningState != (D3D12_RESOURCE_STATES)-1)
				TransitionResource(Resource, Resource.m_TransitioningState);

			D3D12_RESOURCE_STATES OldState = Resource.m_UsageState;

			if (OldState != NewState)
			{
				assert(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
				D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

				BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				BarrierDesc.Transition.pResource = Resource.GetResource();
				BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				BarrierDesc.Transition.StateBefore = OldState;
				BarrierDesc.Transition.StateAfter = NewState;

				BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

				Resource.m_TransitioningState = NewState;
			}

			if (FlushImmediate || m_NumBarriersToFlush == 16)
				FlushResourceBarriers();
		}

		void CommandContext::InsertUAVBarrier(GPUResource& Resource, bool FlushImmediate)
		{
			assert(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
			D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.UAV.pResource = Resource.GetResource();

			if (FlushImmediate)
				FlushResourceBarriers();
		}

		void CommandContext::InsertAliasBarrier(GPUResource& Before, GPUResource& After, bool FlushImmediate)
		{
			assert(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
			D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.Aliasing.pResourceBefore = Before.GetResource();
			BarrierDesc.Aliasing.pResourceAfter = After.GetResource();

			if (FlushImmediate)
				FlushResourceBarriers();
		}

		void CommandContext::CopyBuffer(GPUResource& Dest, GPUResource& Src)
		{
			TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
			TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
			FlushResourceBarriers();
			m_CommandList->CopyResource(Dest.GetResource(), Src.GetResource());
		}

		void CommandContext::CopyBufferRegion(GPUResource& Dest, size_t DestOffset, GPUResource& Src, size_t SrcOffset, size_t NumBytes)
		{
			TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
			//TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
			FlushResourceBarriers();
			m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetResource(), SrcOffset, NumBytes);
		}

		void CommandContext::CopySubresource(GPUResource& Dest, UINT DestSubIndex, GPUResource& Src, UINT SrcSubIndex)
		{
			FlushResourceBarriers();

			D3D12_TEXTURE_COPY_LOCATION DestLocation =
			{
				Dest.GetResource(),
				D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				DestSubIndex
			};

			D3D12_TEXTURE_COPY_LOCATION SrcLocation =
			{
				Src.GetResource(),
				D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
				SrcSubIndex
			};

			m_CommandList->CopyTextureRegion(&DestLocation, 0, 0, 0, &SrcLocation, nullptr);
		}

		void CommandContext::CopyCounter(GPUResource& Dest, size_t DestOffset, StructuredBuffer& Src)
		{
			TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
			TransitionResource(Src.GetCounterBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
			FlushResourceBarriers();
			m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetCounterBuffer().GetResource(), 0, 4);
		}

		void CommandContext::ResetCounter(StructuredBuffer& Buf, uint32_t Value)
		{
			FillBuffer(Buf.GetCounterBuffer(), 0, Value, sizeof(uint32_t));
			TransitionResource(Buf.GetCounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		void CommandContext::InitializeTexture(GPUResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[])
		{
			UINT64 uploadBufferSize = GetRequiredIntermediateSize(Dest.GetResource(), 0, NumSubresources);

			CommandContext& InitContext = CommandContext::Begin();

			// Copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
			DynamicAlloc mem = InitContext.ReserveUploadMemory(uploadBufferSize);
			UpdateSubresources(InitContext.m_CommandList, Dest.GetResource(), mem.Buffer.GetResource(), 0, 0, NumSubresources, SubData);
			InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ);

			// Execute the command list and wait for it to finish so we can release the upload buffer
			InitContext.Finish(true);
		}

		void CommandContext::InitializeBuffer(GPUResource& Dest, const void* Data, size_t NumBytes, size_t Offset)
		{
			CommandContext& InitContext = CommandContext::Begin();

			DynamicAlloc mem = InitContext.ReserveUploadMemory(NumBytes);
			SIMDMemoryCopy(mem.DataPtr, Data, Math::DivideByMultiple(NumBytes, 16));

			// copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
			InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
			InitContext.m_CommandList->CopyBufferRegion(Dest.GetResource(), Offset, mem.Buffer.GetResource(), 0, NumBytes);
			InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

			// Execute the command list and wait for it to finish so we can release the upload buffer
			InitContext.Finish(true);
		}

		void CommandContext::InitializeTextureArraySlice(GPUResource& Dest, UINT SliceIndex, GPUResource& Src)
		{
			CommandContext& Context = CommandContext::Begin();

			Context.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
			Context.FlushResourceBarriers();

			const D3D12_RESOURCE_DESC& DestDesc = Dest.GetResource()->GetDesc();
			const D3D12_RESOURCE_DESC& SrcDesc = Src.GetResource()->GetDesc();

			assert(SliceIndex < DestDesc.DepthOrArraySize&&
				SrcDesc.DepthOrArraySize == 1 &&
				DestDesc.Width == SrcDesc.Width &&
				DestDesc.Height == SrcDesc.Height &&
				DestDesc.MipLevels <= SrcDesc.MipLevels
			);

			UINT SubResourceIndex = SliceIndex * DestDesc.MipLevels;

			for (UINT i = 0; i < DestDesc.MipLevels; ++i)
			{
				D3D12_TEXTURE_COPY_LOCATION destCopyLocation =
				{
					Dest.GetResource(),
					D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
					SubResourceIndex + i
				};

				D3D12_TEXTURE_COPY_LOCATION srcCopyLocation =
				{
					Src.GetResource(),
					D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
					i
				};

				Context.m_CommandList->CopyTextureRegion(&destCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
			}

			Context.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ);
			Context.Finish(true);
		}

		void CommandContext::ReadbackTexture2D(GPUResource& ReadbackBuffer, PixelBuffer& SrcBuffer)
		{
			// The footprint may depend on the device of the resource, but we assume there is only one device.
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedFootprint;
			g_Device->GetCopyableFootprints(&SrcBuffer.GetResource()->GetDesc(), 0, 1, 0, &PlacedFootprint, nullptr, nullptr, nullptr);

			// This very short command list only issues one API call and will be synchronized so we can immediately read
			// the buffer contents.
			CommandContext& Context = CommandContext::Begin(L"Copy texture to memory");

			Context.TransitionResource(SrcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);

			Context.m_CommandList->CopyTextureRegion(
				&CD3DX12_TEXTURE_COPY_LOCATION(ReadbackBuffer.GetResource(), PlacedFootprint), 0, 0, 0,
				&CD3DX12_TEXTURE_COPY_LOCATION(SrcBuffer.GetResource(), 0), nullptr);

			Context.Finish(true);
		}

		void CommandContext::WriteBuffer(GPUResource& Dest, size_t DestOffset, const void* Data, size_t NumBytes)
		{
			assert(Data != nullptr && Math::IsAligned(Data, 16));
			DynamicAlloc TempSpace = m_CPULinearAllocator.Allocate(NumBytes, 512);
			SIMDMemoryCopy(TempSpace.DataPtr, Data, Math::DivideByMultiple(NumBytes, 16));
			CopyBufferRegion(Dest, DestOffset, TempSpace.Buffer, TempSpace.Offset, NumBytes);
		}

		void CommandContext::FillBuffer(GPUResource& Dest, size_t DestOffset, DWParam Value, size_t NumBytes)
		{
			DynamicAlloc TempSpace = m_CPULinearAllocator.Allocate(NumBytes, 512);
			__m128 VectorValue = _mm_set1_ps(Value.Float);
			SIMDMemoryFill(TempSpace.DataPtr, VectorValue, Math::DivideByMultiple(NumBytes, 16));
			CopyBufferRegion(Dest, DestOffset, TempSpace.Buffer, TempSpace.Offset, NumBytes);
		}




#pragma endregion







    }//DirectX12Graphics
}//GlareEngine 

