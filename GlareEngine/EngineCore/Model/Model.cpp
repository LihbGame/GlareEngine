#include "Model.h"
#include "Model/MeshSorter.h"

void GlareEngine::Model::AddToRender(MeshSorter& sorter, const GPUBuffer& meshConstants, const ScaleAndTranslation sphereTransforms[], const Joint* skeleton) const
{
	// Pointer to current mesh
	const uint8_t* pMesh = m_MeshData.get();

	const Frustum& frustum = sorter.GetViewFrustum();
	const AffineTransform& viewMat = (const AffineTransform&)sorter.GetViewMatrix();

	for (uint32_t i = 0; i < m_NumMeshes; ++i)
	{
		const Mesh& mesh = *(const Mesh*)pMesh;

		const ScaleAndTranslation& sphereXform = sphereTransforms[mesh.meshCBV];
		Math::BoundingSphere sphereLS((const XMFLOAT4*)mesh.bounds);
		Math::BoundingSphere sphereWS = sphereXform * sphereLS;
		Math::BoundingSphere sphereVS = Math::BoundingSphere(sphereWS.GetCenter() * viewMat, sphereWS.GetRadius());

		if (frustum.IntersectSphere(sphereVS))
		{
			float distance = -sphereVS.GetCenter().GetZ() - sphereVS.GetRadius();
			sorter.AddMesh(mesh, distance,
				meshConstants.GetGPUVirtualAddress() + sizeof(MeshConstants) * mesh.meshCBV,
				m_MaterialConstants.GetGPUVirtualAddress() + sizeof(MaterialConstants) * mesh.materialCBV,
				m_DataBuffer.GetGPUVirtualAddress(), skeleton);
		}

		pMesh += sizeof(Mesh) + (mesh.numDraws - 1) * sizeof(Mesh::Draw);
	}
}

void GlareEngine::Model::Destroy()
{
	m_BoundingSphere = Math::BoundingSphere(eZero);
	m_DataBuffer.Destroy();
	m_MaterialConstants.Destroy();
	m_NumNodes = 0;
	m_NumMeshes = 0;
	m_MeshData = nullptr;
	m_SceneGraph = nullptr;
}

GlareEngine::ModelInstance::ModelInstance(std::shared_ptr<const Model> sourceModel)
	: m_Model(sourceModel), m_Locator(eIdentity)
{
	static_assert((_alignof(MeshConstants) & 255) == 0, "CBVs need 256 byte alignment");

	if (sourceModel == nullptr)
	{
		m_MeshConstantsCPU.Destroy();
		m_MeshConstantsGPU.Destroy();
		m_BoundingSphereTransforms = nullptr;
		m_AnimGraph = nullptr;
		m_AnimState.clear();
		m_Skeleton = nullptr;
	}
	else
	{
		m_MeshConstantsCPU.Create(L"Mesh Constant Upload Buffer", sourceModel->m_NumNodes * sizeof(MeshConstants));
		m_MeshConstantsGPU.Create(L"Mesh Constant GPU Buffer", sourceModel->m_NumNodes, sizeof(MeshConstants));
		m_BoundingSphereTransforms.reset(new __m128[sourceModel->m_NumNodes]);
		m_Skeleton.reset(new Joint[sourceModel->m_NumJoints]);

		if (sourceModel->m_NumAnimations > 0)
		{
			m_AnimGraph.reset(new GraphNode[sourceModel->m_NumNodes]);
			std::memcpy(m_AnimGraph.get(), sourceModel->m_SceneGraph.get(), sourceModel->m_NumNodes * sizeof(GraphNode));
			m_AnimState.resize(sourceModel->m_NumAnimations);
		}
		else
		{
			m_AnimGraph = nullptr;
			m_AnimState.clear();
		}
	}
}

GlareEngine::ModelInstance::ModelInstance(const ModelInstance& modelInstance)
	: ModelInstance(modelInstance.m_Model)
{
}

ModelInstance& GlareEngine::ModelInstance::operator=(std::shared_ptr<const Model> sourceModel)
{
	return *this;
}

void GlareEngine::ModelInstance::Update(GraphicsContext& gfxContext, float deltaTime)
{
}

void GlareEngine::ModelInstance::Resize(float newRadius)
{
	if (m_Model == nullptr)
		return;

	m_Locator.SetScale(newRadius / m_Model->m_BoundingSphere.GetRadius());
}

Math::Vector3 GlareEngine::ModelInstance::GetCenter() const
{
	if (m_Model == nullptr)
		return Vector3(eOrigin);

	return m_Model->m_BoundingSphere.GetCenter() * m_Locator;
}

Math::Scalar GlareEngine::ModelInstance::GetRadius() const
{
	if (m_Model == nullptr)
		return Scalar(eZero);

	return m_Locator.GetScale() * m_Model->m_BoundingSphere.GetRadius();
}

Math::BoundingSphere GlareEngine::ModelInstance::GetBoundingSphere() const
{
	if (m_Model == nullptr)
		return Math::BoundingSphere(eZero);

	return m_Locator * m_Model->m_BoundingSphere;
}

Math::OrientedBox GlareEngine::ModelInstance::GetBoundingBox() const
{
	if (m_Model == nullptr)
		return AxisAlignedBox(Vector3(eZero), Vector3(eZero));

	return m_Locator * m_Model->m_BoundingBox;
}
