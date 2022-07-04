#pragma once
#include "glTF.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"
#include <cstdint>
#include <string>

namespace GlareEngine
{
    using namespace Math;

    struct Primitive
    {
        BoundingSphere m_BoundsLS;      // local space bounds
        BoundingSphere m_BoundsOS;      // object space bounds
        AxisAlignedBox m_BBoxLS;        // local space AABB
        AxisAlignedBox m_BBoxOS;        // object space AABB
        ByteArray VB;
        ByteArray IB;
        ByteArray DepthVB;
        uint32_t primCount;
        union
        {
            uint32_t hash;
            struct {
                uint32_t psoFlags : 16;
                uint32_t index32 : 1;
                uint32_t materialIdx : 15;
            };
        };
        uint16_t vertexStride;
    };

    void BuildMesh(Primitive& outPrim, const glTF::Primitive& inPrim, const Math::Matrix4& localToObject);
}

