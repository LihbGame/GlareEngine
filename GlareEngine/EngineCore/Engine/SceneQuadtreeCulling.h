#pragma once

#include <cstdint>

#include "Engine/EngineUtility.h"

namespace GlareEngine
{
    enum class SceneCullingResult : uint8_t
    {
        Unknown,
        Culled,
        Intersecting,
        Visible
    };

    struct SceneCullingBounds
    {
        XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
        XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
    };

    struct SceneCullingItem
    {
        uint32_t Id = 0;
        uint32_t Mask = 0xFFFFFFFFu;
        void* UserData = nullptr;
        SceneCullingBounds Bounds;
    };

    struct SceneQuadtreeDebugNode
    {
        SceneCullingBounds Bounds;
        SceneCullingResult Result = SceneCullingResult::Unknown;
        uint32_t Depth = 0;
    };

    class SceneQuadtreeCulling
    {
    public:
        void Build(
            const vector<SceneCullingItem>& Items,
            const SceneCullingBounds& WorldBounds,
            uint32_t MaxDepth = 6,
            uint32_t MaxItemsPerLeaf = 8);

        void Clear();

        void QueryFrustum(
            const XMFLOAT4 FrustumPlanes[6],
            vector<const SceneCullingItem*>& VisibleItems,
            uint32_t Mask = 0xFFFFFFFFu);

        const vector<SceneQuadtreeDebugNode>& GetDebugNodes() const { return mDebugNodes; }
        const SceneCullingBounds& GetWorldBounds() const { return mWorldBounds; }
        bool IsEmpty() const { return mNodes.empty(); }

    private:
        struct Node
        {
            SceneCullingBounds Bounds;
            vector<uint32_t> ItemIndices;
            int Children[4] = { -1, -1, -1, -1 };
            uint32_t Depth = 0;
            SceneCullingResult Result = SceneCullingResult::Unknown;
        };

        int CreateNode(const SceneCullingBounds& Bounds, uint32_t Depth);
        void InsertItem(int NodeIndex, uint32_t ItemIndex);
        void QueryNode(
            int NodeIndex,
            const XMFLOAT4 FrustumPlanes[6],
            vector<const SceneCullingItem*>& VisibleItems,
            uint32_t Mask);
        void CollectNodeItems(int NodeIndex, vector<const SceneCullingItem*>& VisibleItems, uint32_t Mask);
        void RebuildDebugNodes();

        bool FitsChild(const SceneCullingBounds& Bounds, const SceneCullingBounds& ChildBounds) const;
        SceneCullingResult TestBounds(const SceneCullingBounds& Bounds, const XMFLOAT4 FrustumPlanes[6]) const;

        vector<SceneCullingItem> mItems;
        vector<Node> mNodes;
        vector<SceneQuadtreeDebugNode> mDebugNodes;
        SceneCullingBounds mWorldBounds;
        uint32_t mMaxDepth = 6;
        uint32_t mMaxItemsPerLeaf = 8;
    };
}
