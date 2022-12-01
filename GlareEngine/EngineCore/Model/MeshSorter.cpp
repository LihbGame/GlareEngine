#include "MeshSorter.h"
#include "Graphics/Render.h"
#include "Graphics/DepthBuffer.h"

#pragma warning(disable:4319) //zero extending 'uint32_t' to 'uint64_t' of greater size

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
		//Transparent object need revert distance ,From far to near
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
	assert(m_DSV != nullptr);

	context.SetRootSignature(gRootSignature);
	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());
	context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, gSamplerHeap.GetHeapPointer());

	// Set common shader constants
	XMStoreFloat4x4(&globals.ViewProj, m_Camera->GetView() * m_Camera->GetProj());
	globals.EyePosW = m_Camera->GetPosition3f();
	context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &globals);


	//shadow batch
	if (m_BatchType == eShadows)
	{
		context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		context.ClearDepth(*m_DSV);
		context.SetDepthStencilTarget(m_DSV->GetDSV());

		if (m_Viewport.Width == 0)
		{
			m_Viewport.TopLeftX = 0.0f;
			m_Viewport.TopLeftY = 0.0f;
			m_Viewport.Width = (float)m_DSV->GetWidth();
			m_Viewport.Height = (float)m_DSV->GetHeight();
			m_Viewport.MaxDepth = 1.0f;
			m_Viewport.MinDepth = 0.0f;

			m_Scissor.left = 0;
			m_Scissor.right = m_DSV->GetWidth();
			m_Scissor.top = 0;
			m_Scissor.bottom = m_DSV->GetHeight();
		}
	}


}

