#pragma once
#include "GPUBuffer.h"
#include "VectorMath.h"
#include "Camera.h"
#include "CommandContext.h"
#include "UploadBuffer.h"
#include "TextureManager.h"


class MeshSorter
{
public:
	enum BatchType { eDefault, eShadows };
	enum DrawPass { eZPass, eOpaque, eTransparent, eNumPasses };

	MeshSorter(BatchType type)
	{
		m_BatchType = type;
		m_Camera = nullptr;
		m_Viewport = {};
		m_Scissor = {};
		m_NumRTVs = 0;
		m_DSV = nullptr;
		m_SortObjects.clear();
		m_SortKeys.clear();
		std::memset(m_PassCounts, 0, sizeof(m_PassCounts));
		m_CurrentPass = eZPass;
		m_CurrentDraw = 0;
	}

	const Frustum& GetWorldFrustum() const { return m_Camera->GetWorldSpaceFrustum(); }
	const Frustum& GetViewFrustum() const { return m_Camera->GetViewSpaceFrustum(); }
	const Matrix4& GetViewMatrix() const { return m_Camera->GetViewMatrix(); }

	void AddMesh(const Mesh& mesh, float distance,
		D3D12_GPU_VIRTUAL_ADDRESS meshCBV,
		D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
		D3D12_GPU_VIRTUAL_ADDRESS bufferPtr,
		const Joint* skeleton = nullptr);

	void Sort();

	void RenderMeshes(DrawPass pass, GraphicsContext& context, GlobalConstants& globals);

private:

	struct SortKey
	{
		union
		{
			uint64_t value;
			struct
			{
				uint64_t objectIdx : 16;
				uint64_t psoIdx : 12;
				uint64_t key : 32;
				uint64_t passID : 4;
			};
		};
	};

	struct SortObject
	{
		const Mesh* mesh;
		const Joint* skeleton;
		D3D12_GPU_VIRTUAL_ADDRESS meshCBV;
		D3D12_GPU_VIRTUAL_ADDRESS materialCBV;
		D3D12_GPU_VIRTUAL_ADDRESS bufferPtr;
	};

	std::vector<SortObject> m_SortObjects;
	std::vector<uint64_t> m_SortKeys;
	BatchType m_BatchType;
	uint32_t m_PassCounts[eNumPasses];
	DrawPass m_CurrentPass;
	uint32_t m_CurrentDraw;

	const BaseCamera* m_Camera;
	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	uint32_t m_NumRTVs;
	ColorBuffer* m_RTV[8];
	DepthBuffer* m_DSV;
};
