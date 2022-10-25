#pragma once

#include "Model.h"
#include "Animation.h"
#include "Misc/ConstantBuffer.h"
#include "Math/BoundingSphere.h"
#include "Math/BoundingBox.h"

#include <cstdint>
#include <vector>

#define CURRENT_MODEL_FILE_VERSION 1

namespace glTF { class Asset; struct Mesh; }

namespace GlareEngine
{
    using namespace Math;

    // Unaligned mirror of MaterialConstants
    struct MaterialConstantData
    {
        float baseColorFactor[4];       // default=[1,1,1,1]
        float emissiveFactor[3];        // default=[0,0,0]
        float normalTextureScale;       // default=1
        float metallicFactor;           // default=1
        float roughnessFactor;          // default=1
        float heightFactor;             // default=0.001
        uint32_t flags;
    };

    // Used at load time to construct descriptor tables
    struct MaterialTextureData
    {
        uint16_t stringIdx[eNumTextures];
        uint32_t addressModes;
    };

    // All of the information that needs to be written to a [*.Model] data file
    struct glTFModelData
    {
        BoundingSphere m_BoundingSphere;
        AxisAlignedBox m_BoundingBox;
        std::vector<byte> m_GeometryData;
        std::vector<byte> m_AnimationKeyFrameData;
        std::vector<AnimationCurve> m_AnimationCurves;
        std::vector<AnimationSet> m_Animations;
        std::vector<uint16_t> m_JointIndices;
        std::vector<Matrix4> m_JointIBMs;
        std::vector<MaterialTextureData> m_MaterialTextures;
        std::vector<MaterialConstantData> m_MaterialConstants;
        std::vector<Mesh*> m_Meshes;
        std::vector<GraphNode> m_SceneGraph;
        std::vector<std::string> m_TextureNames;
        std::vector<uint8_t> m_TextureOptions;
    };

    struct FileHeader
    {
        char     id[5];   // "Model"
        uint32_t version; // CURRENT_MODEL_FILE_VERSION
        uint32_t numNodes;
        uint32_t numMeshes;
        uint32_t numMaterials;
        uint32_t meshDataSize;
        uint32_t numTextures;
        uint32_t stringTableSize;
        uint32_t geometrySize;
        uint32_t keyFrameDataSize;      // Animation data
        uint32_t numAnimationCurves;
        uint32_t numAnimations;
        uint32_t numJoints;     // All joints for all skins
        float    boundingSphere[4];
        float    minPos[3];
        float    maxPos[3];
    };

    void CompileMesh(
        std::vector<Mesh*>& meshList,
        std::vector<byte>& bufferMemory,
        glTF::Mesh& srcMesh,
        uint32_t matrixIdx,
        const Matrix4& localToObject,
        Math::BoundingSphere& boundingSphere,
        Math::AxisAlignedBox& boundingBox);

    bool BuildModel(glTFModelData& model, const glTF::Asset& asset, int sceneIdx = -1);
    bool SaveModel(const std::wstring& filePath, const glTFModelData& model);

    std::shared_ptr<Model> LoadModel(const std::wstring& filePath, bool forceRebuild = false);

}




