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









    }//DirectX12Graphics
}//GlareEngine 

