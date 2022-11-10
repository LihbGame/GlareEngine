#include "Model.h"
#include "Model/MeshSorter.h"

void GlareEngine::Model::AddToRender(MeshSorter& sorter, const GPUBuffer& meshConstants, const ScaleAndTranslation sphereTransforms[], const Joint* skeleton) const
{
	//// Pointer to current mesh
	//const uint8_t* pMesh = m_MeshData.get();

	//const Frustum& frustum = sorter.GetViewFrustum();
	//const AffineTransform& viewMat = (const AffineTransform&)sorter.GetViewMatrix();

	//for (uint32_t i = 0; i < m_NumMeshes; ++i)
	//{
	//	const Mesh& mesh = *(const Mesh*)pMesh;

	//	const ScaleAndTranslation& sphereXform = sphereTransforms[mesh.meshCBV];
	//	Math::BoundingSphere sphereLS((const XMFLOAT4*)mesh.bounds);
	//	Math::BoundingSphere sphereWS = sphereXform * sphereLS;
	//	Math::BoundingSphere sphereVS = Math::BoundingSphere(viewMat * sphereWS.GetCenter(), sphereWS.GetRadius());

	//	if (frustum.IntersectSphere(sphereVS))
	//	{
	//		float distance = -sphereVS.GetCenter().GetZ() - sphereVS.GetRadius();
	//		sorter.AddMesh(mesh, distance,
	//			meshConstants.GetGPUVirtualAddress() + sizeof(MeshConstants) * mesh.meshCBV,
	//			m_MaterialConstants.GetGPUVirtualAddress() + sizeof(MaterialConstants) * mesh.materialCBV,
	//			m_DataBuffer.GetGPUVirtualAddress(), skeleton);
	//	}

	//	pMesh += sizeof(Mesh) + (mesh.numDraws - 1) * sizeof(Mesh::Draw);
	//}
}

void GlareEngine::Model::Destroy()
{
}

GlareEngine::ModelInstance::ModelInstance(std::shared_ptr<const Model> sourceModel)
{
}

GlareEngine::ModelInstance::ModelInstance(const ModelInstance& modelInstance)
{
}

ModelInstance& GlareEngine::ModelInstance::operator=(std::shared_ptr<const Model> sourceModel)
{
	return *this;
}

void GlareEngine::ModelInstance::Update(GraphicsContext& gfxContext, float deltaTime)
{
}
