#pragma once
#include "EngineUtility.h"
#include "Vertex.h"

namespace GlareEngine
{
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;

	struct MeshData
	{
		std::vector<Vertices::PosNormalTangentTexc> Vertices;
		std::vector<uint32> Indices32;

		std::vector<uint16>& GetIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (size_t i = 0; i < Indices32.size(); ++i)
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
			}

			return mIndices16;
		}

	private:
		std::vector<uint16> mIndices16;
	};




	class ModelMesh
	{
	public:
		ModelMesh() {}
		ModelMesh(ID3D12GraphicsCommandList* CommandList, vector<Vertices::PosNormalTangentTexc> vertices, vector<UINT> indices);
		~ModelMesh();

		MeshGeometry mMeshGeo;
	private:
		void SetupMesh(ID3D12GraphicsCommandList* CommandList);

		vector<Vertices::PosNormalTangentTexc> mVertices;
		vector<UINT> mIndices;
	};
}

