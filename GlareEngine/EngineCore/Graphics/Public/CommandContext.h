#pragma once
#include "L3DUtil.h"
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


class NonCopyable
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

	void Reset(void);

public:

};