#include "Animation.h"

Animation::Animation()
{
}

Animation::~Animation()
{
}

UINT Animation::FindPosition(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    for (UINT i = 0; i < p_node_anim->mNumPositionKeys - 1; i++)
    {
        if (p_animation_time < (float)p_node_anim->mPositionKeys[i + 1].mTime) 
        {
            return i; 
        }
    }
    assert(0);
    return 0;
}

UINT Animation::FindRotation(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    for (UINT i = 0; i < p_node_anim->mNumRotationKeys - 1; i++) 
    {
        if (p_animation_time < (float)p_node_anim->mRotationKeys[i + 1].mTime) 
        {
            return i; 
        }
    }
    assert(0);
    return 0;
}

UINT Animation::FindScaling(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    for (UINT i = 0; i < p_node_anim->mNumScalingKeys - 1; i++) 
    {
        if (p_animation_time < (float)p_node_anim->mScalingKeys[i + 1].mTime) 
        {
            return i; 
        }
    }
    assert(0);
    return 0;
}

const aiNodeAnim* Animation::FindNodeAnim(const aiAnimation* p_animation, const string p_node_name)
{
    // channel in animation contains aiNodeAnim (aiNodeAnim its transformation for bones)
    // numChannels == numBones
    for (UINT i = 0; i < p_animation->mNumChannels; i++)
    {
        const aiNodeAnim* node_anim = p_animation->mChannels[i]; 
        if (string(node_anim->mNodeName.data) == p_node_name)
        {
            return node_anim;
        }
    }

    return nullptr;
}

aiVector3D Animation::CalcInterpolatedPosition(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    if (p_node_anim->mNumPositionKeys == 1) // Keys 
    {
        return p_node_anim->mPositionKeys[0].mValue;
    }

    UINT position_index = FindPosition(p_animation_time, p_node_anim);
    UINT next_position_index = position_index + 1; 
    assert(next_position_index < p_node_anim->mNumPositionKeys);

    float delta_time = (float)(p_node_anim->mPositionKeys[next_position_index].mTime - p_node_anim->mPositionKeys[position_index].mTime);
    
    float factor = (p_animation_time - (float)p_node_anim->mPositionKeys[position_index].mTime) / delta_time;
    assert(factor >= 0.0f && factor <= 1.0f);
    aiVector3D start = p_node_anim->mPositionKeys[position_index].mValue;
    aiVector3D end = p_node_anim->mPositionKeys[next_position_index].mValue;
    aiVector3D delta = end - start;

    return start + factor * delta;
}

aiQuaternion Animation::CalcInterpolatedRotation(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    if (p_node_anim->mNumRotationKeys == 1) // Keys 
    {
        return p_node_anim->mRotationKeys[0].mValue;
    }

    UINT rotation_index = FindRotation(p_animation_time, p_node_anim); 
    UINT next_rotation_index = rotation_index + 1; 
    assert(next_rotation_index < p_node_anim->mNumRotationKeys);
  
    float delta_time = (float)(p_node_anim->mRotationKeys[next_rotation_index].mTime - p_node_anim->mRotationKeys[rotation_index].mTime);

    float factor = (p_animation_time - (float)p_node_anim->mRotationKeys[rotation_index].mTime) / delta_time;

    //cout << "p_node_anim->mRotationKeys[rotation_index].mTime: " << p_node_anim->mRotationKeys[rotation_index].mTime << endl;
    //cout << "p_node_anim->mRotationKeys[next_rotaion_index].mTime: " << p_node_anim->mRotationKeys[next_rotation_index].mTime << endl;
    //cout << "delta_time: " << delta_time << endl;
    //cout << "animation_time: " << p_animation_time << endl;
    //cout << "animation_time - mRotationKeys[rotation_index].mTime: " << (p_animation_time - (float)p_node_anim->mRotationKeys[rotation_index].mTime) << endl;
    //cout << "factor: " << factor << endl << endl << endl;

    assert(factor >= 0.0f && factor <= 1.0f);
    aiQuaternion start_quat = p_node_anim->mRotationKeys[rotation_index].mValue;
    aiQuaternion end_quat = p_node_anim->mRotationKeys[next_rotation_index].mValue;


    return Quatlerp(start_quat, end_quat, factor);
}

aiVector3D Animation::CalcInterpolatedScaling(float p_animation_time, const aiNodeAnim* p_node_anim)
{
    if (p_node_anim->mNumScalingKeys == 1) // Keys
    {
        return p_node_anim->mScalingKeys[0].mValue;
    }

    UINT scaling_index = FindScaling(p_animation_time, p_node_anim); 
    UINT next_scaling_index = scaling_index + 1;
    assert(next_scaling_index < p_node_anim->mNumScalingKeys);
    
    float delta_time = (float)(p_node_anim->mScalingKeys[next_scaling_index].mTime - p_node_anim->mScalingKeys[scaling_index].mTime);
   
    float  factor = (p_animation_time - (float)p_node_anim->mScalingKeys[scaling_index].mTime) / delta_time;
    assert(factor >= 0.0f && factor <= 1.0f);
    aiVector3D start = p_node_anim->mScalingKeys[scaling_index].mValue;
    aiVector3D end = p_node_anim->mScalingKeys[next_scaling_index].mValue;
    aiVector3D delta = end - start;

    return start + factor * delta;
}

void Animation::ReadNodeHierarchy(float p_animation_time, const aiNode* p_node, const aiMatrix4x4 parent_transform)
{
    string node_name(p_node->mName.data);

    const aiAnimation* animation = pAnimeScene->mAnimations[0];
    aiMatrix4x4 node_transform = p_node->mTransformation;

    const aiNodeAnim* node_anim = FindNodeAnim(animation, node_name); 

    if (node_anim)
    {

        //scaling
        aiVector3D scaling_vector = CalcInterpolatedScaling(p_animation_time, node_anim);
        aiMatrix4x4 scaling_matr;
        aiMatrix4x4::Scaling(scaling_vector, scaling_matr);

        //rotation
        aiQuaternion rotate_quat = CalcInterpolatedRotation(p_animation_time, node_anim);
        aiMatrix4x4 rotate_matr = aiMatrix4x4(rotate_quat.GetMatrix());

        //translation
        aiVector3D translate_vector = CalcInterpolatedPosition(p_animation_time, node_anim);
        aiMatrix4x4 translate_matr;
        aiMatrix4x4::Translation(translate_vector, translate_matr);

        if (string(node_anim->mNodeName.data) == string("Head"))
        {
           // aiQuaternion rotate_head = aiQuaternion(rotate_head_xz.w, rotate_head_xz.x, rotate_head_xz.y, rotate_head_xz.z);
       
           // node_transform = translate_matr * (rotate_matr * aiMatrix4x4(rotate_head.GetMatrix())) * scaling_matr;
        }
        else
        {
            node_transform =translate_matr *  rotate_matr*scaling_matr  ;
        }

    }

    aiMatrix4x4 global_transform =parent_transform *node_transform  ;

   
    if (m_bone_mapping.find(node_name) != m_bone_mapping.end()) // true if node_name exist in bone_mapping
    {
        UINT bone_index = m_bone_mapping[node_name];
        m_bone_matrices[bone_index].Final_World_Transform =m_global_inverse_transform  * global_transform *m_bone_matrices[bone_index].Offset_Matrix ;
    }

    for (UINT i = 0; i < p_node->mNumChildren; i++)
    {
        ReadNodeHierarchy(p_animation_time, p_node->mChildren[i], global_transform);
    }

}

void Animation::UpadateBoneTransform(double time_in_sec, vector<XMFLOAT4X4>& transforms)
{
    aiMatrix4x4 identity_matrix; // = mat4(1.0f);

    double time_in_ticks = time_in_sec * ticks_per_second;
    float animation_time = fmod(time_in_ticks, pAnimeScene->mAnimations[0]->mDuration);
    // animation_time 

    ReadNodeHierarchy(animation_time, pAnimeScene->mRootNode, identity_matrix);

    transforms.resize(m_num_bones);

    for (UINT i = 0; i < m_num_bones; i++)
    {
        transforms[i]=AiToXM(m_bone_matrices[i].Final_World_Transform);
        //XMStoreFloat4x4(&transforms[i],XMMatrixTranspose(XMLoadFloat4x4(&transforms[i])));
    }
}

void Animation::SetUpMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList)
{
    const UINT vbByteSize = (UINT)bones_id_weights_for_each_vertex.size() * sizeof(VertexBoneData);

    mBoneGeo.Name = "Bone Mesh";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoneGeo.VertexBufferCPU));
    CopyMemory(mBoneGeo.VertexBufferCPU->GetBufferPointer(), bones_id_weights_for_each_vertex.data(), vbByteSize);

    mBoneGeo.VertexBufferGPU = L3DUtil::CreateDefaultBuffer(dev,
        CommandList, bones_id_weights_for_each_vertex.data(), vbByteSize, mBoneGeo.VertexBufferUploader);

    mBoneGeo.VertexByteStride = sizeof(VertexBoneData);
    mBoneGeo.VertexBufferByteSize = vbByteSize;
}

XMFLOAT4X4 Animation::AiToXM(aiMatrix4x4 ai_matr)
{
    XMFLOAT4X4 lmat;

    lmat._11 = ai_matr.a1;
    lmat._12 = ai_matr.a2;
    lmat._13 = ai_matr.a3;
    lmat._14 = ai_matr.a4;

    lmat._21 = ai_matr.b1;
    lmat._22 = ai_matr.b2;
    lmat._23 = ai_matr.b3;
    lmat._24 = ai_matr.b4;

    lmat._31 = ai_matr.c1;
    lmat._32 = ai_matr.c2;
    lmat._33 = ai_matr.c3;
    lmat._34 = ai_matr.c4;

    lmat._41 = ai_matr.d1;
    lmat._42 = ai_matr.d2;
    lmat._43 = ai_matr.d3;
    lmat._44 = ai_matr.d4;



    return lmat;
}

aiQuaternion Animation::Quatlerp(aiQuaternion a, aiQuaternion b, float blend)
{
    a.Normalize();
    b.Normalize();

    aiQuaternion result;
    float dot_product = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;//cosw
    float one_minus_blend = 1.0f - blend;

    if (dot_product < 0.0f)
    {
        result.x = a.x * one_minus_blend + blend * -b.x;
        result.y = a.y * one_minus_blend + blend * -b.y;
        result.z = a.z * one_minus_blend + blend * -b.z;
        result.w = a.w * one_minus_blend + blend * -b.w;
    }
    else
    {
        result.x = a.x * one_minus_blend + blend * b.x;
        result.y = a.y * one_minus_blend + blend * b.y;
        result.z = a.z * one_minus_blend + blend * b.z;
        result.w = a.w * one_minus_blend + blend * b.w;
    }

    return result.Normalize();
}

void AnimationMesh::SetUpMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList)
{
    const UINT vbByteSize = (UINT)bones_id_weights_for_each_vertex.size() * sizeof(VertexBoneData);

    mBoneGeo.Name = "Bone Mesh";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoneGeo.VertexBufferCPU));
    CopyMemory(mBoneGeo.VertexBufferCPU->GetBufferPointer(), bones_id_weights_for_each_vertex.data(), vbByteSize);

    mBoneGeo.VertexBufferGPU = L3DUtil::CreateDefaultBuffer(dev,
        CommandList, bones_id_weights_for_each_vertex.data(), vbByteSize, mBoneGeo.VertexBufferUploader);

    mBoneGeo.VertexByteStride = sizeof(VertexBoneData);
    mBoneGeo.VertexBufferByteSize = vbByteSize;

    //SubmeshGeometry submesh;
    //submesh.IndexCount = (UINT)indices.size();
    //submesh.StartIndexLocation = 0;
    //submesh.BaseVertexLocation = 0;
    //mBoneGeo.DrawArgs["Model Mesh"] = submesh;
}

void VertexBoneData::addBoneData(int bone_id, float weight)
{
    for (UINT i = 0; i < NUM_BONES_PER_VEREX; i++)
    {
        if (weights[i]==0.0f)
        {
            BoneIds[i] = bone_id;
            weights[i] = weight;
            return;
        }
    }
}
