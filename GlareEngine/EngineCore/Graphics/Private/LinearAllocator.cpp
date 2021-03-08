#include "L3DUtil.h"
#include "LinearAllocator.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include <thread>

using namespace GlareEngine::DirectX12Graphics;

LinearAllocatorType LinearAllocatorPageManager::sm_AutoType = GPUExclusive;
LinearAllocatorPageManager LinearAllocator::sm_PageManager[2];

LinearAllocatorPageManager::LinearAllocatorPageManager()
{
	m_AllocationType = sm_AutoType;
	sm_AutoType = (LinearAllocatorType)(sm_AutoType + 1);
	assert(sm_AutoType <= NumAllocatorTypes);
}

LinearAllocationPage* LinearAllocatorPageManager::RequestPage()
{
	lock_guard<mutex> LockGuard(m_Mutex);

	while (!m_RetiredPages.empty() && g_CommandManager.IsFenceComplete(m_RetiredPages.front().first))
	{
		m_AvailablePages.push(m_RetiredPages.front().second);
		m_RetiredPages.pop();
	}

	LinearAllocationPage* PagePtr = nullptr;

	if (!m_AvailablePages.empty())
	{
		PagePtr = m_AvailablePages.front();
		m_AvailablePages.pop();
	}
	else
	{
		PagePtr = CreateNewPage();
		m_PagePool.emplace_back(PagePtr);
	}

	return PagePtr;
}

void LinearAllocatorPageManager::DiscardPages(uint64_t FenceValue, const vector<LinearAllocationPage*>& UsedPages)
{
	lock_guard<mutex> LockGuard(m_Mutex);
	for (auto iter = UsedPages.begin(); iter != UsedPages.end(); ++iter)
		m_RetiredPages.push(make_pair(FenceValue, *iter));
}

void LinearAllocatorPageManager::FreeLargePages(uint64_t FenceValue, const vector<LinearAllocationPage*>& LargePages)
{
	lock_guard<mutex> LockGuard(m_Mutex);

	while (!m_DeletionQueue.empty() && g_CommandManager.IsFenceComplete(m_DeletionQueue.front().first))
	{
		delete m_DeletionQueue.front().second;
		m_DeletionQueue.pop();
	}

	for (auto iter = LargePages.begin(); iter != LargePages.end(); ++iter)
	{
		(*iter)->Unmap();
		m_DeletionQueue.push(make_pair(FenceValue, *iter));
	}
}