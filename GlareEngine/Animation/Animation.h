#pragma once
//assimp head
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include "L3DUtil.h"

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
    const aiScene* pAnimeScene=nullptr;

    vector<AnimationMesh> mBoneMeshs;


    //anime data
    vector<VertexBoneData> bones_id_weights_for_each_vertex;
    MeshGeometry mBoneGeo;

    map<string, int> m_bone_mapping; // maps a bone name and their index
    int m_num_bones = 0;
    vector<BoneMatrix> m_bone_matrices;

    aiMatrix4x4 m_global_inverse_transform;
    //int m_bone_location[MAX_BONES];
    float ticks_per_second = 0.0f;

};
