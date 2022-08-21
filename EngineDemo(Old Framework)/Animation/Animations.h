#pragma once
//assimp head
#include "AnimEvaluator.h"
#include "EngineUtility.h"
#include "ModelMesh.h"

#define MAX_BONES 100
#define NUM_BONES_PER_VEREX 4

struct BoneMatrix
{
	aiMatrix4x4 Offset_Matrix;
	aiMatrix4x4 Final_World_Transform;
};

struct VertexBoneData
{
	float weights[NUM_BONES_PER_VEREX];
	int BoneIds[NUM_BONES_PER_VEREX];
   

	VertexBoneData()
	{
		//init
		memset(BoneIds, 0, sizeof(BoneIds));
		memset(weights, 0, sizeof(weights));
	}

	void addBoneData(int bone_id, float weight);
};



//һ�����ṹ��ƥ�䳡���Ľڵ�ṹ.
struct SceneAnimNode {
	std::string mName;
	SceneAnimNode* mParent;
	std::vector<SceneAnimNode*> mChildren;

	//�������ľֲ��任
	aiMatrix4x4 mLocalTransform;

	//һ��������������ռ�
	aiMatrix4x4 mGlobalTransform;

	//��ǰ������ͨ�������е������� -1�����δ���ö�������
	int mChannelIndex;

	//Ĭ�Ϲ���
	SceneAnimNode()
		: mName()
		, mParent(nullptr)
		, mChildren()
		, mLocalTransform()
		, mGlobalTransform()
		, mChannelIndex(-1) {
		// empty
	}

	//�������ƵĹ���
	SceneAnimNode(const std::string& pName)
		: mName(pName)
		, mParent(nullptr)
		, mChildren()
		, mLocalTransform()
		, mGlobalTransform()
		, mChannelIndex(-1) {
		// empty
	}

	//�ݹ����������ӽڵ�
	~SceneAnimNode() {
		for (std::vector<SceneAnimNode*>::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
			delete* it;
		}
	}
};

class AnimationMesh
{
public:
	AnimationMesh() {}
	~AnimationMesh() {}
 
	void SetUpMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList);
public:
	//anime data
	std::vector<VertexBoneData> bones_id_weights_for_each_vertex;

	::MeshGeometry mBoneGeo;
};

class Animation
{
public:
	Animation();
	~Animation();



	///animation functions
	UINT FindPosition(float p_animation_time, const aiNodeAnim* p_node_anim);
	UINT FindRotation(float p_animation_time, const aiNodeAnim* p_node_anim);
	UINT FindScaling(float p_animation_time, const aiNodeAnim* p_node_anim);
	const aiNodeAnim* FindNodeAnim(const aiAnimation* p_animation, const std::string p_node_name);
	// calculate transform matrix
	aiVector3D CalcInterpolatedPosition(float p_animation_time, const aiNodeAnim* p_node_anim);
	aiQuaternion CalcInterpolatedRotation(float p_animation_time, const aiNodeAnim* p_node_anim);
	aiVector3D CalcInterpolatedScaling(float p_animation_time, const aiNodeAnim* p_node_anim);

	void ReadNodeHierarchy(float p_animation_time, const aiNode* p_node, const aiMatrix4x4 parent_transform);
	void UpadateBoneTransform(double time_in_sec, std::vector<XMFLOAT4X4>& transforms);

	void SetUpMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList);

	XMFLOAT4X4 AiToXM(aiMatrix4x4 ai_matr);

	aiQuaternion Quatlerp(aiQuaternion a, aiQuaternion b, float blend);



public:
	void InitAnime();

	//���㳡���Ľڵ�ת�������ô˷����Ի�ȡ���½����Ȼ���ٵ�������һ��getter��
	//@param pTime ��ǰʱ�䡣 ���������ⷶΧ��
	void Calculate(double pTime);

   
	//���������ڵ�����±���ת�����󡣾���ԭʼ�ڵ��ת������һ�������صľ����ڽڵ�ĸ��ڵ�ı��ؿռ��С� 
	//����ýڵ�δ���ö������򽫷��ظýڵ��ԭʼת�����Ա������԰�ȫ��ʹ�øýڵ������������ 
	//���û�и������ƵĽڵ㣬�򷵻���ݾ��� ÿ������Calculate����ʱ������ת��������¡�
	//@param pNode�ڵ�
	//@return�Խڵ��������ľֲ�ת����������á�
	const aiMatrix4x4& GetLocalTransform(const aiNode* node) const;

 
	//���������ڵ������ȫ��ת�����󡣷��صľ���λ������ռ��У�������ռ��볡���ĸ��ڵ�ı任��ͬ��
	//����ýڵ�δ���ö������򽫷��ظýڵ��ԭʼ�任���Ա������԰�ȫ��ʹ���������������� 
	//���û�и������ƵĽڵ㣬�򷵻���ݾ��� ÿ������Calculate����ʱ������ת��������¡�
	//@param pNodeName�ڵ�����
	//@return�Խڵ���������ȫ��ת����������á�
	const aiMatrix4x4& GetGlobalTransform(const aiNode* node) const;

   
	//�����������Ĺ�������ÿ���������󶼴Ӱ������е�����ռ�ת��Ϊ��Ƥ�����е�����ռ䣬������������������������
	//��ˣ��ڶ�����ɫ����ʹ�õĳ��þ�����Ϊ��@code boneMatrix * worldMatrix * viewMatrix * projMatrix @endcode
	//@param pNode��������Ľڵ㡣
	//@param pMeshIndex�ڵ�����������������������NODE���������У������ǳ������������У� ʡ��ʹ�ýڵ�ĵ�һ������ͨ��Ҳ��Ψһ��һ����
	//@return�ԹǾ������������á�ֱ����һ�ε���GetBoneMatrices����; Ϊֹ��
	void GetBoneMatrices(const aiNode* pNode,
		size_t pMeshIndex = 0);

	//�ݹ鴴���뵱ǰ�����Ͷ���ƥ����ڲ��ڵ�ṹ��
	SceneAnimNode* CreateNodeTree(aiNode* pNode, SceneAnimNode* pParent);

	//�Ӹ����ľ�������ݹ�����ڲ��ڵ�ת��
	void UpdateTransforms(SceneAnimNode* pNode, const std::vector<aiMatrix4x4>& pTransforms);

	//��������ڲ��ڵ��ȫ��ת������
	void CalculateGlobalTransform(SceneAnimNode* pInternalNode);


public:
	const aiScene* pAnimeScene=nullptr;

	std::vector<AnimationMesh> mBoneMeshs;


	//anime data
	std::vector<VertexBoneData> bones_id_weights_for_each_vertex;
	::MeshGeometry mBoneGeo;

	std::map<std::string, int> m_bone_mapping; // maps a bone name and their index
	UINT m_num_bones = 0;
	std::vector<BoneMatrix> m_bone_matrices;

	aiMatrix4x4 m_global_inverse_transform;
	//int m_bone_location[MAX_BONES];
	float ticks_per_second = 0.0f;

	//�����������㵱ǰ�����ĵ�ǰ���Ƶ�AnimEvaluator
	AnimEvaluator* mAnimEvaluator = nullptr;

	//�ڲ������ṹ�ĸ��ڵ�
	SceneAnimNode* mRootNode = nullptr;

	//���Ƶ��ڵ��ӳ�䣬��ͨ�����ƿ����ҵ��ڵ�
	typedef std::map<const aiNode*, SceneAnimNode*>  NodeMap;
	NodeMap mNodesByName;

	//���Ƶ��ڵ��ӳ�䣬��ͨ�����ƿ����ҵ����������Ľڵ�
	 std::map<const char*, const aiNode*> mBoneNodesByName;


	//�����ڲ�ת����������顣
	std::vector<aiMatrix4x4> mTransforms;
};
