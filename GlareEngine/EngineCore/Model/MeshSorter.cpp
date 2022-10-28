#include "MeshSorter.h"
#include "Graphics/Render.h"

#pragma warning(disable:4319) // '~': zero extending 'uint32_t' to 'uint64_t' of greater size

using namespace GlareEngine::Render;

void MeshSorter::AddMesh(const Mesh& mesh, float distance, 
	D3D12_GPU_VIRTUAL_ADDRESS meshCBV, D3D12_GPU_VIRTUAL_ADDRESS materialCBV, 
	D3D12_GPU_VIRTUAL_ADDRESS bufferPtr, const Joint* skeleton)
{
	SortKey key;
	key.value = m_SortObjects.size();

	bool alphaBlend = (mesh.psoFlags & ModelPSOFlags::eAlphaBlend) == ModelPSOFlags::eAlphaBlend;
	bool alphaTest = (mesh.psoFlags & ModelPSOFlags::eAlphaTest) == ModelPSOFlags::eAlphaTest;
	bool skinned = (mesh.psoFlags & ModelPSOFlags::eHasSkin) == ModelPSOFlags::eHasSkin;

	uint64_t depthPSO = (skinned ? 2 : 0) + (alphaTest ? 1 : 0);

	union float_or_int 
	{ 
		float f; 
		uint32_t u; 
	} dist;

	dist.f = Max(distance, 0.0f);

	if (m_BatchType == eShadows)
	{
		//shadow do not have alpha blend
		if (alphaBlend)
			return;

		key.passID = eZPass;
		key.psoIdx = depthPSO + 4;
		key.key = dist.u;
		m_SortKeys.push_back(key.value);
		m_PassCounts[eZPass]++;
	}
	else if (mesh.psoFlags & ModelPSOFlags::eAlphaBlend)
	{
		key.passID = eTransparent;
		key.psoIdx = mesh.pso;
		//Transparent object need revert distance
		key.key = ~dist.u;
		m_SortKeys.push_back(key.value);
		m_PassCounts[eTransparent]++;
	}
	else if (SeparateZPass || alphaTest)
	{
		key.passID = eZPass;
		key.psoIdx = depthPSO;
		key.key = dist.u;
		m_SortKeys.push_back(key.value);
		m_PassCounts[eZPass]++;

		key.passID = eOpaque;
		key.psoIdx = mesh.pso + 1;
		key.key = dist.u;
		m_SortKeys.push_back(key.value);
		m_PassCounts[eOpaque]++;
	}
	else
	{
		key.passID = eOpaque;
		key.psoIdx = mesh.pso;
		key.key = dist.u;
		m_SortKeys.push_back(key.value);
		m_PassCounts[eOpaque]++;
	}

	SortObject object = { &mesh, skeleton, meshCBV, materialCBV, bufferPtr };
	m_SortObjects.push_back(object);

}

void MeshSorter::Sort()
{
	struct { bool operator()(uint64_t a, uint64_t b) const { return a < b; } } Cmp;
	std::sort(m_SortKeys.begin(), m_SortKeys.end(), Cmp);
}

void MeshSorter::RenderMeshes(DrawPass pass, GraphicsContext& context, MainConstants& globals)
{

}

