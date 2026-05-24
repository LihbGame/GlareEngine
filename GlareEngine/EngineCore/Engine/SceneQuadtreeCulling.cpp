#include "SceneQuadtreeCulling.h"

#include <algorithm>
#include <cmath>

namespace GlareEngine
{
    namespace
    {
        float PlaneDistance(const XMFLOAT4& Plane, const XMFLOAT3& Point)
        {
            return Plane.x * Point.x + Plane.y * Point.y + Plane.z * Point.z + Plane.w;
        }

    }

    void SceneQuadtreeCulling::Build(
        const vector<SceneCullingItem>& Items,
        const SceneCullingBounds& WorldBounds,
        uint32_t MaxDepth,
        uint32_t MaxItemsPerLeaf)
    {
        Clear();

        mItems = Items;
        mWorldBounds = WorldBounds;
        mMaxDepth = (std::max)(1u, MaxDepth);
        mMaxItemsPerLeaf = (std::max)(1u, MaxItemsPerLeaf);

        if (mItems.empty())
        {
            return;
        }

        const int rootIndex = CreateNode(mWorldBounds, 0);
        for (uint32_t itemIndex = 0; itemIndex < (uint32_t)mItems.size(); ++itemIndex)
        {
            InsertItem(rootIndex, itemIndex);
        }

        RebuildDebugNodes();
    }

    void SceneQuadtreeCulling::Clear()
    {
        mItems.clear();
        mNodes.clear();
        mDebugNodes.clear();
        mWorldBounds = {};
    }

    int SceneQuadtreeCulling::CreateNode(const SceneCullingBounds& Bounds, uint32_t Depth)
    {
        Node node;
        node.Bounds = Bounds;
        node.Depth = Depth;
        mNodes.push_back(node);
        return (int)mNodes.size() - 1;
    }

    bool SceneQuadtreeCulling::FitsChild(
        const SceneCullingBounds& Bounds,
        const SceneCullingBounds& ChildBounds) const
    {
        return Bounds.Min.x >= ChildBounds.Min.x && Bounds.Max.x <= ChildBounds.Max.x &&
               Bounds.Min.z >= ChildBounds.Min.z && Bounds.Max.z <= ChildBounds.Max.z;
    }

    void SceneQuadtreeCulling::InsertItem(int NodeIndex, uint32_t ItemIndex)
    {
        const SceneCullingBounds nodeBounds = mNodes[NodeIndex].Bounds;
        const uint32_t nodeDepth = mNodes[NodeIndex].Depth;

        auto insertIntoChild = [&](uint32_t targetItemIndex) -> bool
        {
            const SceneCullingBounds& itemBounds = mItems[targetItemIndex].Bounds;
            const float midX = 0.5f * (nodeBounds.Min.x + nodeBounds.Max.x);
            const float midZ = 0.5f * (nodeBounds.Min.z + nodeBounds.Max.z);

            SceneCullingBounds childBounds[4] = {};
            childBounds[0] = { { nodeBounds.Min.x, nodeBounds.Min.y, nodeBounds.Min.z }, { midX, nodeBounds.Max.y, midZ } };
            childBounds[1] = { { midX, nodeBounds.Min.y, nodeBounds.Min.z }, { nodeBounds.Max.x, nodeBounds.Max.y, midZ } };
            childBounds[2] = { { nodeBounds.Min.x, nodeBounds.Min.y, midZ }, { midX, nodeBounds.Max.y, nodeBounds.Max.z } };
            childBounds[3] = { { midX, nodeBounds.Min.y, midZ }, { nodeBounds.Max.x, nodeBounds.Max.y, nodeBounds.Max.z } };

            for (int child = 0; child < 4; ++child)
            {
                if (FitsChild(itemBounds, childBounds[child]))
                {
                    if (mNodes[NodeIndex].Children[child] < 0)
                    {
                        const int childIndex = CreateNode(childBounds[child], nodeDepth + 1);
                        mNodes[NodeIndex].Children[child] = childIndex;
                    }
                    InsertItem(mNodes[NodeIndex].Children[child], targetItemIndex);
                    return true;
                }
            }
            return false;
        };

        if (nodeDepth >= mMaxDepth)
        {
            mNodes[NodeIndex].ItemIndices.push_back(ItemIndex);
            return;
        }

        const bool hasChildren =
            mNodes[NodeIndex].Children[0] >= 0 ||
            mNodes[NodeIndex].Children[1] >= 0 ||
            mNodes[NodeIndex].Children[2] >= 0 ||
            mNodes[NodeIndex].Children[3] >= 0;

        if (hasChildren && insertIntoChild(ItemIndex))
        {
            return;
        }

        if (mNodes[NodeIndex].ItemIndices.size() < mMaxItemsPerLeaf)
        {
            mNodes[NodeIndex].ItemIndices.push_back(ItemIndex);
            return;
        }

        vector<uint32_t> pendingItems = mNodes[NodeIndex].ItemIndices;
        pendingItems.push_back(ItemIndex);
        mNodes[NodeIndex].ItemIndices.clear();

        for (uint32_t pendingItem : pendingItems)
        {
            if (!insertIntoChild(pendingItem))
            {
                mNodes[NodeIndex].ItemIndices.push_back(pendingItem);
            }
        }
    }

    SceneCullingResult SceneQuadtreeCulling::TestBounds(
        const SceneCullingBounds& Bounds,
        const XMFLOAT4 FrustumPlanes[6]) const
    {
        bool fullyInside = true;

        const XMFLOAT3 center =
        {
            0.5f * (Bounds.Min.x + Bounds.Max.x),
            0.5f * (Bounds.Min.y + Bounds.Max.y),
            0.5f * (Bounds.Min.z + Bounds.Max.z)
        };
        const XMFLOAT3 extents =
        {
            0.5f * (Bounds.Max.x - Bounds.Min.x),
            0.5f * (Bounds.Max.y - Bounds.Min.y),
            0.5f * (Bounds.Max.z - Bounds.Min.z)
        };

        for (int i = 0; i < 6; ++i)
        {
            const XMFLOAT4& plane = FrustumPlanes[i];
            const float radius =
                extents.x * std::abs(plane.x) +
                extents.y * std::abs(plane.y) +
                extents.z * std::abs(plane.z);
            const float distance = PlaneDistance(plane, center);

            if (distance + radius < 0.0f)
            {
                return SceneCullingResult::Culled;
            }

            if (distance - radius < 0.0f)
            {
                fullyInside = false;
            }
        }

        return fullyInside ? SceneCullingResult::Visible : SceneCullingResult::Intersecting;
    }

    void SceneQuadtreeCulling::QueryFrustum(
        const XMFLOAT4 FrustumPlanes[6],
        vector<const SceneCullingItem*>& VisibleItems,
        uint32_t Mask)
    {
        VisibleItems.clear();

        for (Node& node : mNodes)
        {
            node.Result = SceneCullingResult::Unknown;
        }

        if (!mNodes.empty())
        {
            QueryNode(0, FrustumPlanes, VisibleItems, Mask);
        }

        RebuildDebugNodes();
    }

    void SceneQuadtreeCulling::QueryNode(
        int NodeIndex,
        const XMFLOAT4 FrustumPlanes[6],
        vector<const SceneCullingItem*>& VisibleItems,
        uint32_t Mask)
    {
        Node& node = mNodes[NodeIndex];
        node.Result = TestBounds(node.Bounds, FrustumPlanes);

        if (node.Result == SceneCullingResult::Culled)
        {
            return;
        }

        if (node.Result == SceneCullingResult::Visible)
        {
            CollectNodeItems(NodeIndex, VisibleItems, Mask);
            return;
        }

        for (uint32_t itemIndex : node.ItemIndices)
        {
            const SceneCullingItem& item = mItems[itemIndex];
            if ((item.Mask & Mask) == 0)
            {
                continue;
            }

            if (TestBounds(item.Bounds, FrustumPlanes) != SceneCullingResult::Culled)
            {
                VisibleItems.push_back(&item);
            }
        }

        for (int child : node.Children)
        {
            if (child >= 0)
            {
                QueryNode(child, FrustumPlanes, VisibleItems, Mask);
            }
        }
    }

    void SceneQuadtreeCulling::CollectNodeItems(
        int NodeIndex,
        vector<const SceneCullingItem*>& VisibleItems,
        uint32_t Mask)
    {
        Node& node = mNodes[NodeIndex];
        node.Result = SceneCullingResult::Visible;

        for (uint32_t itemIndex : node.ItemIndices)
        {
            const SceneCullingItem& item = mItems[itemIndex];
            if ((item.Mask & Mask) != 0)
            {
                VisibleItems.push_back(&item);
            }
        }

        for (int child : node.Children)
        {
            if (child >= 0)
            {
                CollectNodeItems(child, VisibleItems, Mask);
            }
        }
    }

    void SceneQuadtreeCulling::RebuildDebugNodes()
    {
        mDebugNodes.clear();
        mDebugNodes.reserve(mNodes.size());

        for (const Node& node : mNodes)
        {
            SceneQuadtreeDebugNode debugNode;
            debugNode.Bounds = node.Bounds;
            debugNode.Result = node.Result;
            debugNode.Depth = node.Depth;
            mDebugNodes.push_back(debugNode);
        }
    }
}
