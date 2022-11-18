//  This is an implementation of Tom Forsyth's "Linear-Speed Vertex Cache 
//  Optimization" algorithm as described here:
//  https://tomforsyth1000.github.io/papers/fast_vert_cache_opt.html

#include <stdint.h>
#include <ASSERT.h>
#include <math.h>
#include <algorithm>
#include <memory>

#include "IndexOptimize.h"

using namespace std;

namespace GlareEngine
{
	// code for computing vertex score was taken, as much as possible
	// directly from the original publication.
	float ComputeVertexCacheScore(int cachePosition, size_t vertexCacheSize)
	{
		const float FindVertexScore_CacheDecayPower = 1.5f;
		const float FindVertexScore_LastTriScore = 0.75f;

		float score = 0.0f;
		if (cachePosition < 0)
		{
			// Vertex is not in FIFO cache - no score.
		}
		else
		{
			if (cachePosition < 3)
			{
				// This vertex was used in the last triangle,
				// so it has a fixed score, whichever of the three
				// it's in. Otherwise, you can get very different
				// answers depending on whether you add
				// the triangle 1,2,3 or 3,1,2 - which is silly.
				score = FindVertexScore_LastTriScore;
			}
			else
			{
				assert(cachePosition < int(vertexCacheSize));
				// Points for being high in the cache.
				const float scaler = 1.0f / (vertexCacheSize - 3);
				score = 1.0f - (cachePosition - 3) * scaler;
				score = powf(score, FindVertexScore_CacheDecayPower);
			}
		}

		return score;
	}

	float ComputeVertexValenceScore(size_t numActiveFaces)
	{
		const float FindVertexScore_ValenceBoostScale = 2.0f;
		const float FindVertexScore_ValenceBoostPower = 0.5f;

		float score = 0.0f;

		// Bonus points for having a low number of tris still to
		// use the vert, so we get rid of lone verts quickly.
		float valenceBoost = powf(static_cast<float>(numActiveFaces),
			-FindVertexScore_ValenceBoostPower);
		score += FindVertexScore_ValenceBoostScale * valenceBoost;

		return score;
	}


	enum
	{
		eMaxVertexCacheSize = 64,
		eMaxPrecomputedVertexValenceScores = 64
	};

	float s_vertexCacheScores[eMaxVertexCacheSize + 1][eMaxVertexCacheSize];
	float s_vertexValenceScores[eMaxPrecomputedVertexValenceScores];

	bool ComputeVertexScores()
	{
		for (uint32_t cacheSize = 0; cacheSize <= eMaxVertexCacheSize; ++cacheSize)
		{
			for (uint32_t cachePos = 0; cachePos < cacheSize; ++cachePos)
			{
				s_vertexCacheScores[cacheSize][cachePos] = ComputeVertexCacheScore(cachePos, cacheSize);
			}
		}

		for (uint32_t valence = 0; valence < eMaxPrecomputedVertexValenceScores; ++valence)
		{
			s_vertexValenceScores[valence] = ComputeVertexValenceScore(valence);
		}

		return true;
	}
	bool s_vertexScoresComputed = ComputeVertexScores();

	inline float FindVertexCacheScore(size_t cachePosition, size_t maxSizeVertexCache)
	{
		return s_vertexCacheScores[maxSizeVertexCache][cachePosition];
	}

	inline float FindVertexValenceScore(size_t numActiveTris)
	{
		return s_vertexValenceScores[numActiveTris];
	}

	float FindVertexScore(size_t numActiveFaces, size_t cachePosition, size_t vertexCacheSize)
	{
		//ASSERT(s_vertexScoresComputed);

		if (numActiveFaces == 0)
		{
			// No tri needs this vertex!
			return -1.0f;
		}

		float score = 0.0f;
		if (cachePosition < vertexCacheSize)
		{
			score += s_vertexCacheScores[vertexCacheSize][cachePosition];
		}

		if (numActiveFaces < eMaxPrecomputedVertexValenceScores)
		{
			score += s_vertexValenceScores[numActiveFaces];
		}
		else
		{
			score += ComputeVertexValenceScore(numActiveFaces);
		}

		return score;
	}

	struct OptimizeVertexData
	{
		float   score;
		size_t  activeFaceListStart;
		size_t  activeFaceListSize;
		size_t  cachePos0;
		size_t  cachePos1;
		OptimizeVertexData() : score(0.0f), activeFaceListStart(0), activeFaceListSize(0), cachePos0(0), cachePos1(0) { }
	};

	template <typename IndexType>
	struct IndexSortCompareIndexed
	{
		const IndexType* _indexData;

		IndexSortCompareIndexed(const IndexType* indexData)
			: _indexData(indexData)
		{
		}

		bool operator()(size_t a, size_t b) const
		{
			IndexType indexA = _indexData[a];
			IndexType indexB = _indexData[b];

			return indexA < indexB;
		}
	};

	struct FaceValenceSort
	{
		const OptimizeVertexData* _vertexData;

		FaceValenceSort(const OptimizeVertexData* vertexData)
			: _vertexData(vertexData)
		{
		}

		bool operator()(size_t a, size_t b) const
		{
			const OptimizeVertexData* vA0 = _vertexData + a * 3 + 0;
			const OptimizeVertexData* vA1 = _vertexData + a * 3 + 1;
			const OptimizeVertexData* vA2 = _vertexData + a * 3 + 2;
			const OptimizeVertexData* vB0 = _vertexData + b * 3 + 0;
			const OptimizeVertexData* vB1 = _vertexData + b * 3 + 1;
			const OptimizeVertexData* vB2 = _vertexData + b * 3 + 2;

			size_t aValence = vA0->activeFaceListSize + vA1->activeFaceListSize + vA2->activeFaceListSize;
			size_t bValence = vB0->activeFaceListSize + vB1->activeFaceListSize + vB2->activeFaceListSize;

			// higher scoring faces are those with lower valence totals reverse sort (reverse of reverse)
			return aValence < bValence;
		}
	};


	template <typename SrcIndexType, typename DstIndexType>
	void OptimizeFaces(const SrcIndexType* indexList, size_t indexCount, DstIndexType* newIndexList, size_t cacheSize)
	{
		assert(cacheSize <= eMaxVertexCacheSize);

		unique_ptr<OptimizeVertexData[]> vertexDataList(new OptimizeVertexData[indexCount]); // upper bounds on size is indexCount
		unique_ptr<uint32_t[]> vertexRemap(new uint32_t[indexCount]);
		unique_ptr<uint32_t[]> activeFaceList(new uint32_t[indexCount]);

		size_t faceCount = indexCount / 3;
		unique_ptr<uint8_t[]> processedFaceList(new uint8_t[faceCount]);
		memset(processedFaceList.get(), 0, sizeof(uint8_t) * faceCount);
		unique_ptr<uint32_t[]> faceSorted(new uint32_t[faceCount]);
		unique_ptr<uint32_t[]> faceReverseLookup(new uint32_t[faceCount]);

		// build the vertex remap table
		uint32_t uniqueVertexCount = 0;
		{
			typedef IndexSortCompareIndexed<SrcIndexType> indexSorter;
			uint32_t* indexSorted = new uint32_t[indexCount];

			for (uint32_t i = 0; i < indexCount; i++)
			{
				indexSorted[i] = i;
			}

			indexSorter sortFunc(indexList);
			std::sort(indexSorted, indexSorted + indexCount, sortFunc);

			for (uint32_t i = 0; i < indexCount; i++)
			{
				if (i == 0 || sortFunc(indexSorted[i - 1], indexSorted[i]))
				{
					// it's not a duplicate
					vertexRemap[indexSorted[i]] = uniqueVertexCount;
					uniqueVertexCount++;
				}
				else
				{
					vertexRemap[indexSorted[i]] = vertexRemap[indexSorted[i - 1]];
				}
			}

			delete[] indexSorted;
		}

		// compute face count per vertex
		for (size_t i = 0; i < indexCount; ++i)
		{
			vertexDataList[vertexRemap[i]].activeFaceListSize++;
		}

		const size_t kEvictedCacheIndex = std::numeric_limits<DstIndexType>::max();
		{
			// allocate face list per vertex
			size_t curActiveFaceListPos = 0;
			for (uint32_t i = 0; i < uniqueVertexCount; ++i)
			{
				OptimizeVertexData& vertexData = vertexDataList[i];
				vertexData.cachePos0 = kEvictedCacheIndex;
				vertexData.cachePos1 = kEvictedCacheIndex;
				vertexData.activeFaceListStart = curActiveFaceListPos;
				curActiveFaceListPos += vertexData.activeFaceListSize;
				vertexData.score = FindVertexScore(vertexData.activeFaceListSize, vertexData.cachePos0, cacheSize);
				vertexData.activeFaceListSize = 0;
			}
			assert(curActiveFaceListPos == indexCount);
		}

		// sort unprocessed faces by highest score
		for (uint32_t f = 0; f < faceCount; f++)
		{
			faceSorted[f] = f;
		}
		FaceValenceSort faceValenceSort(vertexDataList.get());
		std::sort(faceSorted.get(), faceSorted.get() + faceCount, faceValenceSort);
		for (uint32_t f = 0; f < faceCount; f++)
		{
			faceReverseLookup[faceSorted[f]] = f;
		}

		// fill out face list per vertex
		for (uint32_t i = 0; i < indexCount; i += 3)
		{
			for (size_t j = 0; j < 3; ++j)
			{
				OptimizeVertexData& vertexData = vertexDataList[vertexRemap[i + j]];
				activeFaceList[vertexData.activeFaceListStart + vertexData.activeFaceListSize] = i;
				vertexData.activeFaceListSize++;
			}
		}

		uint32_t vertexCacheBuffer[(eMaxVertexCacheSize + 3) * 2];
		uint32_t* cache0 = vertexCacheBuffer;
		uint32_t* cache1 = vertexCacheBuffer + eMaxVertexCacheSize + 3;
		size_t entriesInCache0 = 0;

		uint32_t bestFace = 0;
		float bestScore = -1.0f;

		const float maxValenceScore = FindVertexScore(1, kEvictedCacheIndex, cacheSize) * 3.0f;

		uint32_t nextBestFace = 0;
		for (uint32_t i = 0; i < indexCount; i += 3)
		{
			if (bestScore < 0.0f)
			{
				// no verts in the cache are used by any unprocessed faces so
				// search all unprocessed faces for a new starting point
				for (; nextBestFace < faceCount; nextBestFace++)
				{
					uint32_t faceIndex = faceSorted[nextBestFace];
					if (processedFaceList[faceIndex] == 0)
					{
						uint32_t face = faceIndex * 3;
						float faceScore = 0.0f;
						for (uint32_t k = 0; k < 3; ++k)
						{
							//ASSERT(vertexData.activeFaceListSize > 0);
							//ASSERT(vertexData.cachePos0 >= lruCacheSize);

							float vertexScore = vertexDataList[vertexRemap[face + k]].score;
							faceScore += vertexScore;
						}

						bestScore = faceScore;
						bestFace = face;

						nextBestFace++;
						break; // we're searching a pre-sorted list, first one we find will be the best
					}
				}
				assert(bestScore >= 0.0f);
			}

			processedFaceList[bestFace / 3] = 1;
			size_t entriesInCache1 = 0;

			// add bestFace to LRU cache and to newIndexList
			for (uint32_t v = 0; v < 3; ++v)
			{
				uint32_t index = indexList[bestFace + v];
				newIndexList[i + v] = (DstIndexType)index;

				OptimizeVertexData& vertexData = vertexDataList[vertexRemap[bestFace + v]];

				if (vertexData.cachePos1 >= entriesInCache1)
				{
					vertexData.cachePos1 = entriesInCache1;
					cache1[entriesInCache1++] = vertexRemap[bestFace + v];

					if (vertexData.activeFaceListSize == 1)
					{
						--vertexData.activeFaceListSize;
						continue;
					}
				}

				assert(vertexData.activeFaceListSize > 0);
				uint32_t* begin = activeFaceList.get() + vertexData.activeFaceListStart;
				uint32_t* end = begin + vertexData.activeFaceListSize;
				uint32_t* it = std::find(begin, end, bestFace);
				assert(it != end);
				std::swap(*it, *(end - 1));
				--vertexData.activeFaceListSize;
				vertexData.score = FindVertexScore(vertexData.activeFaceListSize, vertexData.cachePos1, cacheSize);

				// need to re-sort the faces that use this vertex, as their score will change due to activeFaceListSize shrinking
				for (uint32_t* fi = begin; fi != end - 1; ++fi)
				{
					uint32_t faceIndex = *fi / 3;

					uint32_t n = faceReverseLookup[faceIndex];
					assert(faceSorted[n] == faceIndex);

					// found it, now move it up
					for (n; n > 0; --n)
					{
						if (!faceValenceSort(n, n - 1))
							break;

						faceReverseLookup[faceSorted[n]] = n - 1;
						faceReverseLookup[faceSorted[n - 1]] = n;
						std::swap(faceSorted[n], faceSorted[n - 1]);
					}
				}
			}

			// move the rest of the old verts in the cache down and compute their new scores
			for (uint32_t c0 = 0; c0 < entriesInCache0; ++c0)
			{
				OptimizeVertexData& vertexData = vertexDataList[cache0[c0]];

				if (vertexData.cachePos1 >= entriesInCache1)
				{
					vertexData.cachePos1 = entriesInCache1;
					cache1[entriesInCache1++] = cache0[c0];
					vertexData.score = FindVertexScore(vertexData.activeFaceListSize, vertexData.cachePos1, cacheSize);
					// don't need to re-sort this vertex... once it gets out of the cache, it'll have its original score
				}
			}

			// find the best scoring triangle in the current cache (including up to 3 that were just evicted)
			bestScore = -1.0f;
			for (uint32_t c1 = 0; c1 < entriesInCache1; ++c1)
			{
				OptimizeVertexData& vertexData = vertexDataList[cache1[c1]];
				vertexData.cachePos0 = vertexData.cachePos1;
				vertexData.cachePos1 = kEvictedCacheIndex;
				for (uint32_t j = 0; j < vertexData.activeFaceListSize; ++j)
				{
					uint32_t face = activeFaceList[vertexData.activeFaceListStart + j];
					float faceScore = 0.0f;
					for (uint32_t v = 0; v < 3; ++v)
					{
						faceScore += vertexDataList[vertexRemap[face + v]].score;
					}
					if (faceScore > bestScore)
					{
						bestScore = faceScore;
						bestFace = face;
					}
				}
			}

			std::swap(cache0, cache1);
			entriesInCache0 = std::min(entriesInCache1, cacheSize);
		}
	}
}