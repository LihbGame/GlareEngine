#pragma once
#include <stdint.h>
namespace GlareEngine
{
	//Parameters:
	//      indexList
	//          input index list
	//      indexCount
	//          the number of indices in the list
	//      newIndexList
	//          a pointer to a preallocated buffer the same size as indexList to hold the optimized index list
	//      cacheSize
	//          the size of the simulated post-transform cache (max:64)

	template <typename SrcIndexType, typename DstIndexType>
	void OptimizeFaces(const SrcIndexType* indexList, size_t indexCount, DstIndexType* newIndexList, size_t cacheSize);

	template void OptimizeFaces<uint16_t, uint16_t>(const uint16_t* indexList, size_t indexCount, uint16_t* newIndexList, size_t cacheSize);
	template void OptimizeFaces<uint32_t, uint16_t>(const uint32_t* indexList, size_t indexCount, uint16_t* newIndexList, size_t cacheSize);
	template void OptimizeFaces<uint32_t, uint32_t>(const uint32_t* indexList, size_t indexCount, uint32_t* newIndexList, size_t cacheSize);

}

