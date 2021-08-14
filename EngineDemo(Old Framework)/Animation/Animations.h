#pragma once
//assimp head
#include "AnimEvaluator.h"
#include "EngineUtility.h"


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



//一个树结构以匹配场景的节点结构.
struct SceneAnimNode {
    std::string mName;
    SceneAnimNode* mParent;
    std::vector<SceneAnimNode*> mChildren;

    //最近计算的局部变换
    aiMatrix4x4 mLocalTransform;

    //一样，但是在世界空间
    aiMatrix4x4 mGlobalTransform;

    //当前动画的通道数组中的索引。 -1（如果未设置动画）。
    int mChannelIndex;

    //默认构造
    SceneAnimNode()
        : mName()
        , mParent(nullptr)
        , mChildren()
        , mLocalTransform()
        , mGlobalTransform()
        , mChannelIndex(-1) {
        // empty
    }

    //给定名称的构造
    SceneAnimNode(const std::string& pName)
        : mName(pName)
        , mParent(nullptr)
        , mChildren()
        , mLocalTransform()
        , mGlobalTransform()
        , mChannelIndex(-1) {
        // empty
    }

    //递归销毁所有子节点
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
    vector<VertexBoneData> bones_id_weights_for_each_vertex;

    MeshGeometry mBoneGeo;
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
    const aiNodeAnim* FindNodeAnim(const aiAnimation* p_animation, const string p_node_name);
    // calculate transform matrix
    aiVector3D CalcInterpolatedPosition(float p_animation_time, const aiNodeAnim* p_node_anim);
    aiQuaternion CalcInterpolatedRotation(float p_animation_time, const aiNodeAnim* p_node_anim);
    aiVector3D CalcInterpolatedScaling(float p_animation_time, const aiNodeAnim* p_node_anim);

    void ReadNodeHierarchy(float p_animation_time, const aiNode* p_node, const aiMatrix4x4 parent_transform);
    void UpadateBoneTransform(double time_in_sec, vector<XMFLOAT4X4>& transforms);

    void SetUpMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList);

    XMFLOAT4X4 AiToXM(aiMatrix4x4 ai_matr);

    aiQuaternion Quatlerp(aiQuaternion a, aiQuaternion b, float blend);



public:
    void InitAnime();

    //计算场景的节点转换。调用此方法以获取最新结果，然后再调用其中一个getter。
    //@param pTime 当前时间。 可以是任意范围。
    void Calculate(double pTime);

   
    //检索给定节点的最新本地转换矩阵。就像原始节点的转换矩阵一样，返回的矩阵在节点的父节点的本地空间中。 
    //如果该节点未设置动画，则将返回该节点的原始转换，以便您可以安全地使用该节点或将其分配给自身。 
    //如果没有给定名称的节点，则返回身份矩阵。 每当调用Calculate（）时，所有转换都会更新。
    //@param pNode节点
    //@return对节点最近计算的局部转换矩阵的引用。
    const aiMatrix4x4& GetLocalTransform(const aiNode* node) const;

 
    //检索给定节点的最新全局转换矩阵。返回的矩阵位于世界空间中，该世界空间与场景的根节点的变换相同。
    //如果该节点未设置动画，则将返回该节点的原始变换，以便您可以安全地使用它或将其分配给自身。 
    //如果没有给定名称的节点，则返回身份矩阵。 每当调用Calculate（）时，所有转换都会更新。
    //@param pNodeName节点名称
    //@return对节点最近计算的全局转换矩阵的引用。
    const aiMatrix4x4& GetGlobalTransform(const aiNode* node) const;

   
    //计算给定网格的骨骼矩阵。每个骨骼矩阵都从绑定姿势中的网格空间转换为蒙皮姿势中的网格空间，它不包含网格物体的世界矩阵。
    //因此，在顶点着色器中使用的常用矩阵链为：@code boneMatrix * worldMatrix * viewMatrix * projMatrix @endcode
    //@param pNode承载网格的节点。
    //@param pMeshIndex节点的网格阵列中网格的索引。NODE的网格阵列，而不是场景的网格阵列！ 省略使用节点的第一个网格，通常也是唯一的一个。
    //@return对骨矩阵向量的引用。直到下一次调用GetBoneMatrices（）; 为止。
    void GetBoneMatrices(const aiNode* pNode,
        size_t pMeshIndex = 0);

    //递归创建与当前场景和动画匹配的内部节点结构。
    SceneAnimNode* CreateNodeTree(aiNode* pNode, SceneAnimNode* pParent);

    //从给定的矩阵数组递归更新内部节点转换
    void UpdateTransforms(SceneAnimNode* pNode, const std::vector<aiMatrix4x4>& pTransforms);

    //计算给定内部节点的全局转换矩阵
    void CalculateGlobalTransform(SceneAnimNode* pInternalNode);


public:
    const aiScene* pAnimeScene=nullptr;

    vector<AnimationMesh> mBoneMeshs;


    //anime data
    vector<VertexBoneData> bones_id_weights_for_each_vertex;
    MeshGeometry mBoneGeo;

    map<string, int> m_bone_mapping; // maps a bone name and their index
    UINT m_num_bones = 0;
    vector<BoneMatrix> m_bone_matrices;

    aiMatrix4x4 m_global_inverse_transform;
    //int m_bone_location[MAX_BONES];
    float ticks_per_second = 0.0f;

    //我们用来计算当前动画的当前姿势的AnimEvaluator
    AnimEvaluator* mAnimEvaluator = nullptr;

    //内部场景结构的根节点
    SceneAnimNode* mRootNode = nullptr;

    //名称到节点的映射，可通过名称快速找到节点
    typedef std::map<const aiNode*, SceneAnimNode*>  NodeMap;
    NodeMap mNodesByName;

    //名称到节点的映射，可通过名称快速找到给定骨骼的节点
     std::map<const char*, const aiNode*> mBoneNodesByName;


    //返回内部转换结果的数组。
    std::vector<aiMatrix4x4> mTransforms;
};
