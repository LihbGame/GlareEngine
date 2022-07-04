#pragma once
#include "Animation.h"
#include "GPUBuffer.h"
#include "VectorMath.h"
#include "Camera.h"
#include "CommandContext.h"
#include "UploadBuffer.h"
#include "TextureManager.h"
#include "BoundingBox.h"
#include "BoundingSphere.h"

namespace GlareEngine
{
	namespace PSOFlags
	{
		enum : uint16_t
		{
			eHasPosition = 0x001,
			eHasNormal = 0x002,
			eHasTangent = 0x004,
			eHasUV0 = 0x008,
			eHasUV1 = 0x010,
			eAlphaBlend = 0x020,
			eAlphaTest = 0x040,
			eTwoSided = 0x080,
			eHasSkin = 0x100,
		};
	}

	struct Mesh
	{
		float    bounds[4];     // A bounding sphere
		//Vertex
		uint32_t vbOffset;      // BufferLocation - Buffer.GPUVirtualAddress
		uint32_t vbSize;        // SizeInBytes
		//Depth
		uint32_t vbDepthOffset; // BufferLocation - Buffer.GPUVirtualAddress
		uint32_t vbDepthSize;   // SizeInBytes
		//Index
		uint32_t ibOffset;      // BufferLocation - Buffer.GPUVirtualAddress
		uint32_t ibSize;        // SizeInBytes

		uint8_t  vbStride;      // StrideInBytes
		uint8_t  ibFormat;      // DXGI_FORMAT

		uint16_t meshCBV;       // Index of mesh constant buffer
		uint16_t materialCBV;   // Index of material constant buffer

		uint16_t srvTable;      // Offset into SRV descriptor heap for textures
		uint16_t samplerTable;  // Offset into sampler descriptor heap for samplers

		uint16_t psoFlags;      // Flags needed to request a PSO
		uint16_t pso;           // Index of pipeline state object

		uint16_t numJoints;     // Number of skeleton joints when skinning
		uint16_t startJoint;    // Flat offset to first joint index

		uint16_t numDraws;      // Number of draw groups

		struct Draw
		{
			uint32_t primCount;   // Number of indices = 3 * number of triangles
			uint32_t startIndex;  // Offset to first index in index buffer 
			uint32_t baseVertex;  // Offset to first vertex in vertex buffer
		};
		Draw draw[1];           // Actually 1 or more draws
	};


	struct GraphNode // 96 bytes
	{
		Matrix4			transform;
		Quaternion		rotation;
		XMFLOAT3		scale;

		uint32_t		matrixIdx : 28;
		uint32_t		hasSibling : 1;
		uint32_t		hasChildren : 1;
		uint32_t		staleMatrix : 1;
		uint32_t		skeletonRoot : 1;
	};

	struct Joint
	{
		Matrix4 positionTransform;
		Matrix3 normalTransform;
	};

	class Model
	{
	public:

		~Model() { Destroy(); }

		/*void Render(MeshSorter& sorter,
			const GPUBuffer& meshConstants,
			const ScaleAndTranslation sphereTransforms[],
			const Joint* skeleton) const;*/

		Math::BoundingSphere	m_BoundingSphere; // Object-space bounding sphere
		AxisAlignedBox			m_BoundingBox;
		ByteAddressBuffer		m_DataBuffer;
		ByteAddressBuffer		m_MaterialConstants;
		uint32_t	m_NumNodes;
		uint32_t	m_NumMeshes;
		uint32_t	m_NumAnimations;
		uint32_t	m_NumJoints;
		std::unique_ptr<uint8_t[]>			m_MeshData;
		std::unique_ptr<GraphNode[]>		m_SceneGraph;
		std::vector<Texture*>				m_Textures;
		std::unique_ptr<uint8_t[]>			m_KeyFrameData;
		std::unique_ptr<AnimationCurve[]>	m_CurveData;
		std::unique_ptr<AnimationSet[]>		m_Animations;
		std::unique_ptr<uint16_t[]>			m_JointIndices;
		std::unique_ptr<Matrix4[]>			m_JointIBMs;

	protected:
		void Destroy();
	};

	class ModelInstance
	{
	public:
		ModelInstance() {}
		~ModelInstance() {
			m_MeshConstantsCPU.Destroy();
			m_MeshConstantsGPU.Destroy();
		}
		ModelInstance(std::shared_ptr<const Model> sourceModel);
		ModelInstance(const ModelInstance& modelInstance);

		ModelInstance& operator=(std::shared_ptr<const Model> sourceModel);

		bool IsNull(void) const { return m_Model == nullptr; }

		void Update(GraphicsContext& Context, float deltaTime);
		//void Render(Renderer::MeshSorter& sorter) const;

		void Resize(float newRadius);
		Math::Vector3 GetCenter() const;
		Math::Scalar GetRadius() const;
		Math::BoundingSphere GetBoundingSphere() const;
		Math::OrientedBox GetBoundingBox() const;

		size_t GetNumAnimations(void) const { return m_AnimState.size(); }
		void PlayAnimation(uint32_t animIdx, bool loop);
		void PauseAnimation(uint32_t animIdx);
		void ResetAnimation(uint32_t animIdx);
		void StopAnimation(uint32_t animIdx);
		void UpdateAnimations(float deltaTime);
		void LoopAllAnimations(void);

	private:
		std::shared_ptr<const Model> m_Model;
		UploadBuffer m_MeshConstantsCPU;
		ByteAddressBuffer m_MeshConstantsGPU;
		std::unique_ptr<__m128[]> m_BoundingSphereTransforms;
		Math::UniformTransform m_Locator;

		std::unique_ptr<GraphNode[]> m_AnimGraph;   // A copy of the scene graph when instancing animation
		std::vector<AnimationState> m_AnimState;    // Per-animation (not per-curve)
		std::unique_ptr<Joint[]> m_Skeleton;
	};
}