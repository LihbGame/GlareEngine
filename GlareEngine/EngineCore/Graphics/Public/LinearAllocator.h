#pragma once

#include "GPUResource.h"
#include <vector>
#include <queue>
#include <mutex>


// 常量块必须是16个常量的倍数，每个常量为16字节。
#define DEFAULT_ALIGN 256
namespace GlareEngine {
	namespace DirectX12Graphics {
		//  if you are unsure.
		struct DynAlloc
		{
			//各种类型的分配可能包含NULL指针。 在解除引用之前，请检查
			DynAlloc(GPUResource& BaseResource, size_t ThisOffset, size_t ThisSize)
				: Buffer(BaseResource), Offset(ThisOffset), Size(ThisSize) {}

			GPUResource& Buffer;      // 与该内存相关联的D3D缓冲区。
			size_t Offset;            // 缓冲区资源开始的偏移量.
			size_t Size;              // 本次分配的保留大小
			void* DataPtr;            // CPU可写地址
			D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;    // GPU可见地址
		};


		class LinearAllocator
		{
		};
	}
}
