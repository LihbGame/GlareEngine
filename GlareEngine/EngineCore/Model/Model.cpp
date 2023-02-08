#include "Model.h"
#include "Model/MeshSorter.h"

void GlareEngine::Model::AddToRender(MeshSorter& sorter, const GPUBuffer& meshConstants, const ScaleAndTranslation sphereTransforms[], const Joint* skeleton) const
{
	// Pointer to current mesh
	const uint8_t* pMesh = m_MeshData.get();

	const Frustum& frustum = sorter.GetViewFrustum();
	AffineTransform& viewMat = (AffineTransform&)sorter.GetViewMatrix();

	for (uint32_t i = 0; i < m_NumMeshes; ++i)
	{
		const Mesh& mesh = *(const Mesh*)pMesh;

		const ScaleAndTranslation& sphereXform = sphereTransforms[mesh.meshCBV];
		Math::BoundingSphere sphereLS((const XMFLOAT4*)mesh.bounds);

		if (m_IsNegetiveZForward)
		{
			Vector3 center = -sphereLS.GetCenter();
			center = center * (Quaternion)XMMatrixRotationZ(MathHelper::Pi);
			sphereLS.SetCenter(center);
		}

		Math::BoundingSphere sphereWS = sphereXform * sphereLS;


		//Matrix4 view;
		//if (sorter.m_BatchType == MeshSorter::eShadows)
		//{
		//	view = (Matrix4)(XMMatrixTranspose((XMMATRIX)sorter.GetViewMatrix()));
		//	viewMat = (AffineTransform&)view;
		//}


		Math::BoundingSphere sphereVS = Math::BoundingSphere(sphereWS.GetCenter() * viewMat, sphereWS.GetRadius());

		if (frustum.IntersectSphere(sphereVS))
		{
			//Objects close to the camera are sorted first
			float distance = -sphereVS.GetCenter().GetZ() - sphereVS.GetRadius();

			//add mesh to rendering
			sorter.AddMesh(mesh, distance,
				meshConstants.GetGPUVirtualAddress() + sizeof(MeshConstants) * mesh.meshCBV,
				m_MaterialConstants.GetGPUVirtualAddress() + sizeof(MaterialConstants) * mesh.materialCBV,
				m_DataBuffer.GetGPUVirtualAddress(), skeleton);
		}
		//to next mesh
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

GlareEngine::ModelInstance::ModelInstance(std::shared_ptr<Model> sourceModel,float Scale,bool IsNegetiveZForward)
	: m_Model(sourceModel), m_Locator(eIdentity)
{
	static_assert((_alignof(MeshConstants) & 255) == 0, "CBVs need 256 byte alignment");

	m_Model->SetModelVerticeOrder(IsNegetiveZForward);

	if (IsNegetiveZForward)
	{
		//The forward of our coordinate system is +z. If it is a -z-oriented model, 
		//use reverse coordinates to change the order of model vertices.
		m_Locator.SetScale(Scalar(Vector3(-Scale)));
		m_Locator.SetRotation((Quaternion)XMMatrixRotationZ(MathHelper::Pi));
	}
	else
	{
		m_Locator.SetScale(Scalar(Vector3(Scale)));
	}

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

ModelInstance& GlareEngine::ModelInstance::operator=(std::shared_ptr<Model> sourceModel)
{
	m_Model = sourceModel;
	m_Locator = UniformTransform(eIdentity);
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
	return *this;
}

void GlareEngine::ModelInstance::Update(GraphicsContext& gfxContext, float deltaTime)
{
	if (m_Model == nullptr)
		return;

	static const size_t MaxStackDepth = 32;

	size_t stackIdx = 0;
	Matrix4 matrixStack[MaxStackDepth];
	Matrix4 ParentMatrix = Matrix4((AffineTransform)m_Locator);

	ScaleAndTranslation* boundingSphereTransforms = (ScaleAndTranslation*)m_BoundingSphereTransforms.get();
	MeshConstants* ConstBuffer = (MeshConstants*)m_MeshConstantsCPU.Map();

	if (m_AnimGraph)
	{
		//update animation
		UpdateAnimations(deltaTime);

		for (uint32_t i = 0; i < m_Model->m_NumNodes; ++i)
		{
			GraphNode& node = m_AnimGraph[i];

			// Regenerate the 3x3 matrix if it has scale or rotation
			if (node.staleMatrix)
			{
				node.staleMatrix = false;
				node.transform.Set3x3(Matrix3(node.rotation) * Matrix3::MakeScale(node.scale));
			}
		}
	}

	//Get GraphNode
	const GraphNode* sceneGraph = m_AnimGraph ? m_AnimGraph.get() : m_Model->m_SceneGraph.get();

	// Traverse the scene graph in depth first order.  This is the same as linear order
	// for how the nodes are stored in memory.  Uses a matrix stack instead of recursion.
	for (const GraphNode* Node = sceneGraph; ; ++Node)
	{
		Matrix4 xform = Node->transform;
		if (!Node->skeletonRoot)
			xform = xform * ParentMatrix;

		// Concatenate the transform with the parent's matrix and update the matrix list
		{
			// Scoped so that I don't forget that I'm pointing to write-combined memory and should not read from it.
			MeshConstants& cbv = ConstBuffer[Node->matrixIdx];
			cbv.World = xform;
			cbv.WorldIT = InverseTranspose(xform.Get3x3());

			Scalar scaleXSqr = LengthSquare((Vector3)xform.GetX());
			Scalar scaleYSqr = LengthSquare((Vector3)xform.GetY());
			Scalar scaleZSqr = LengthSquare((Vector3)xform.GetZ());
			Scalar sphereScale = Sqrt(Max(Max(scaleXSqr, scaleYSqr), scaleZSqr));

			boundingSphereTransforms[Node->matrixIdx] = ScaleAndTranslation((Vector3)xform.GetW(), sphereScale);
		}

		// If the next node will be a Child, replace the parent matrix with our new matrix
		if (Node->hasChildren)
		{
			//if we have siblings, make sure to backup our current parent matrix on the stack
			if (Node->hasSibling)
			{
				//Overflowed the matrix stack
				assert(stackIdx < MaxStackDepth);
				matrixStack[stackIdx++] = ParentMatrix;
			}
			ParentMatrix = xform;
		}
		else if (!Node->hasSibling)
		{
			// There are no more siblings.  If the stack is empty, we are done.  Otherwise, pop a matrix off the stack and continue.
			if (stackIdx == 0)
				break;

			ParentMatrix = matrixStack[--stackIdx];
		}
	}

	// Update skeletal joints
	for (uint32_t i = 0; i < m_Model->m_NumJoints; ++i)
	{
		Joint& joint = m_Skeleton[i];
		joint.positionTransform = m_Model->m_JointIBMs[i] * ConstBuffer[m_Model->m_JointIndices[i]].World;
		joint.normalTransform = InverseTranspose(joint.positionTransform.Get3x3());
	}

	m_MeshConstantsCPU.Unmap();

	gfxContext.TransitionResource(m_MeshConstantsGPU, D3D12_RESOURCE_STATE_COPY_DEST, true);
	gfxContext.GetCommandList()->CopyBufferRegion(m_MeshConstantsGPU.GetResource(), 0, m_MeshConstantsCPU.GetResource(), 0, m_MeshConstantsCPU.GetBufferSize());
	gfxContext.TransitionResource(m_MeshConstantsGPU, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void GlareEngine::ModelInstance::AddToRender(MeshSorter& sorter) const
{
	if (m_Model != nullptr)
	{
		m_Model->AddToRender(sorter, m_MeshConstantsGPU, (const ScaleAndTranslation*)m_BoundingSphereTransforms.get(),
			m_Skeleton.get());
	}
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

	return m_Model->m_BoundingBox * m_Locator;
}

Math::AxisAlignedBox GlareEngine::ModelInstance::GetAxisAlignedBox() const
{
	return m_Model.get()->m_BoundingBox;
}
