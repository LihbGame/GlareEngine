#include "Animation.h"

Animation::Animation()
{
}

Animation::~Animation()
{
}

UINT Animation::FindPosition(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    return 0;
}

UINT Animation::FindRotation(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    return 0;
}

UINT Animation::FindScaling(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    return 0;
}

const aiNodeAnim* Animation::FindNodeAnim(const aiAnimation* p_animation, const string p_node_name)
{
    return nullptr;
}

aiVector3D Animation::CalcInterpolatedPosition(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    return aiVector3D();
}

aiQuaternion Animation::CalcInterpolatedRotation(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    return aiQuaternion();
}

aiVector3D Animation::CalcInterpolatedScaling(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    return aiVector3D();
}

void Animation::ReadNodeHierarchy(float p_animation_time, const aiNode* p_node, const aiMatrix4x4 parent_transform)
{
}

void Animation::BoneTransform(double time_in_sec, vector<aiMatrix4x4>& transforms)
{
}

void VertexBoneData::addBoneData(int bone_id, float weight)
{
}
