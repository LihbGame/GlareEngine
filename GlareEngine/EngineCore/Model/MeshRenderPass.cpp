#include "MeshRenderPass.h"
#include "Graphics/Render.h"
#include "Graphics/DepthBuffer.h"
#include "Graphics/BufferManager.h"
#include "InstanceModel/glTFInstanceModel.h"
#include "Shadow/ShadowMap.h"

#pragma warning(disable:4319) //zero extending 'uint32_t' to 'uint64_t' of greater size

using namespace GlareEngine::Render;

void MeshRenderPass::AddMesh(const Mesh& mesh, float distance,
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

void MeshRenderPass::Sort()
{
	struct { bool operator()(uint64_t a, uint64_t b) const { return a < b; } } Cmp;
	std::sort(m_SortKeys.begin(), m_SortKeys.end(), Cmp);
}

void MeshRenderPass::RenderMeshes(DrawPass pass, GraphicsContext& context, MainConstants globals)
{
	assert(m_DSV != nullptr);

	context.SetRootSignature(gRootSignature);
	context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, gTextureHeap.GetHeapPointer());
	context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, gSamplerHeap.GetHeapPointer());

	// Set common shader constants
	//XMStoreFloat4x4(&globals.ViewProj, m_Camera->GetView() * m_Camera->GetProj());
	globals.EyePosW = m_Camera->GetPosition3f();

	if (m_BatchType == eShadows)
	{
		if (dynamic_cast<ShadowCamera*>(m_Camera))
		{
			globals.ViewProj = dynamic_cast<ShadowCamera*>(m_Camera)->GetShadowMap()->GetViewProj();
		}
		else
		{
			globals.ViewProj = m_Camera->GetViewProj();
		}
	}

	context.SetDynamicConstantBufferView((int)RootSignatureType::eMainConstantBuffer, sizeof(MainConstants), &globals);


	//shadow batch
	if (m_BatchType == eShadows)
	{
		context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		context.ClearDepthAndStencil(*m_DSV, 1.0f);
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
	else
	{
		for (uint32_t i = 0; i < m_NumRTVs; ++i)
		{
			assert(m_RTV[i] != nullptr);
			assert(m_DSV->GetWidth() == m_RTV[i]->GetWidth());
			assert(m_DSV->GetHeight() == m_RTV[i]->GetHeight());
		}

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

	for (; m_CurrentPass <= pass; m_CurrentPass = (DrawPass)(m_CurrentPass + 1))
	{
		const uint32_t passCount = m_PassCounts[m_CurrentPass];
		if (passCount == 0)
			continue;

		if (m_BatchType == eDefault)
		{
			switch (m_CurrentPass)
			{
			case eZPass:
				context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				context.SetDepthStencilTarget(m_DSV->GetDSV());
				break;
			case eOpaque:
				if (SeparateZPass)
				{
					context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_DEPTH_READ);
					context.TransitionResource(*m_RTV[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
					if (Render::gRasterRenderPipelineType == TBFR || Render::gRasterRenderPipelineType == CBFR)
					{
						context.SetRenderTarget(m_RTV_Descrptor[0], m_DSV->GetDSV_DepthReadOnly());
					}
					else if (Render::gRasterRenderPipelineType == TBDR || Render::gRasterRenderPipelineType == CBDR)
					{
						context.SetRenderTargets(Render::GBUFFER_Count, Render::GetGBufferRTV(context), m_DSV->GetDSV_DepthReadOnly());
					}
				}
				else
				{
					context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_DEPTH_WRITE);
					context.TransitionResource(*m_RTV[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
					context.SetRenderTarget(m_RTV_Descrptor[0], m_DSV->GetDSV());

					if (Render::gRasterRenderPipelineType == TBFR || Render::gRasterRenderPipelineType == CBFR)
					{
						context.SetRenderTarget(m_RTV_Descrptor[0], m_DSV->GetDSV());
					}
					else if (Render::gRasterRenderPipelineType == TBDR || Render::gRasterRenderPipelineType == CBDR)
					{
						context.SetRenderTargets(Render::GBUFFER_Count, Render::GetGBufferRTV(context), m_DSV->GetDSV());
					}
				}
				break;
			case eTransparent:
				context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_DEPTH_READ);
				context.TransitionResource(*m_RTV[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
				context.SetRenderTarget(m_RTV_Descrptor[0], m_DSV->GetDSV_DepthReadOnly());
				break;
			}
		}

		context.SetViewportAndScissor(m_Viewport, m_Scissor);
		context.FlushResourceBarriers();

		const uint32_t lastDraw = m_CurrentDraw + passCount;

		//finish draw current pass
		while (m_CurrentDraw < lastDraw)
		{
			SortKey key;
			//get the key from Sorted Keys
			key.value = m_SortKeys[m_CurrentDraw];
			//use key get object 
			const SortObject& object = m_SortObjects[key.objectIdx];
			const Mesh& mesh = *object.mesh;

			context.SetConstantBuffer((UINT)RootSignatureType::eCommonConstantBuffer, object.meshCBV);
			context.SetConstantBuffer((UINT)RootSignatureType::eMaterialConstants, object.materialCBV);
			context.SetDescriptorTable((UINT)RootSignatureType::eMaterialSRVs, gTextureHeap[mesh.srvTable]);
			context.SetDescriptorTable((UINT)RootSignatureType::eMaterialSamplers, gSamplerHeap[mesh.samplerTable]);
			if (mesh.numJoints > 0)
			{
				//Unspecified joint matrix array
				assert(object.skeleton != nullptr);
				context.SetDynamicSRV((UINT)RootSignatureType::eSkinMatrices, sizeof(Joint) * mesh.numJoints, object.skeleton + mesh.startJoint);
			}
			//Set PSO
			PSOProxy* psoProxy = glTFInstanceModel::gModelPSOs[key.psoIdx].GetProxy();
			if (psoProxy)
			{
				context.SetPipelineState(GET_PSO(glTFInstanceModel::gModelPSOs[key.psoIdx]));
			}
			else
				context.SetPipelineState(glTFInstanceModel::gModelPSOs[key.psoIdx]);

			//Set Vertex Buffer
			if (m_CurrentPass == eZPass)
			{
				bool alphaTest = (mesh.psoFlags & ModelPSOFlags::eAlphaTest) == ModelPSOFlags::eAlphaTest;
				//if have UV data
				uint32_t stride = alphaTest ? 16u : 12u;
				//if have Joints data
				if (mesh.numJoints > 0)
					stride += 16;
				context.SetVertexBuffer(0, { object.bufferPtr + mesh.vbDepthOffset, mesh.vbDepthSize, stride });
			}
			else
			{
				context.SetVertexBuffer(0, { object.bufferPtr + mesh.vbOffset, mesh.vbSize, mesh.vbStride });
			}

			//Set Index Buffer
			context.SetIndexBuffer({ object.bufferPtr + mesh.ibOffset, mesh.ibSize, (DXGI_FORMAT)mesh.ibFormat });

			//Draw Call
			for (uint32_t i = 0; i < mesh.numDraws; ++i)
				context.DrawIndexed(mesh.draw[i].primCount, mesh.draw[i].startIndex, mesh.draw[i].baseVertex);

			//To next Draw call
			++m_CurrentDraw;
		}
	}

	//change shadow map state to srv
	if (m_BatchType == eShadows)
	{
		context.TransitionResource(*m_DSV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

