#pragma once

#include "VectorMath.h"
#include "Transform.h"
namespace GlareEngine
{
    namespace Math
    {
        class AxisAlignedBox
        {
        public:
            AxisAlignedBox() : m_min(FLT_MAX, FLT_MAX, FLT_MAX), m_max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
            AxisAlignedBox(EZeroTag) : m_min(FLT_MAX, FLT_MAX, FLT_MAX), m_max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}
            AxisAlignedBox(Vector3 min, Vector3 max) : m_min(min), m_max(max) {}

            void AddPoint(Vector3 point)
            {
                m_min = Min(point, m_min);
                m_max = Max(point, m_max);
            }

            void AddBoundingBox(const AxisAlignedBox& box)
            {
                AddPoint(box.m_min);
                AddPoint(box.m_max);
            }

            AxisAlignedBox Union(const AxisAlignedBox& box)
            {
                return AxisAlignedBox(Min(m_min, box.m_min), Max(m_max, box.m_max));
            }

            Vector3 GetMin() const { return m_min; }
            Vector3 GetMax() const { return m_max; }
            Vector3 GetCenter() const { return (m_min + m_max) * 0.5f; }
            Vector3 GetDimensions() const { return Max(m_max - m_min, Vector3(eZero)); }

        private:

            Vector3 m_min;
            Vector3 m_max;
        };

        class OrientedBox
        {
        public:
            OrientedBox() {}

            OrientedBox(const AxisAlignedBox& box)
            {
                m_repr.SetBasis(Matrix3::MakeScale(box.GetMax() - box.GetMin()));
                m_repr.SetTranslation(box.GetMin());
            }

            friend OrientedBox operator* (const OrientedBox& obb, const AffineTransform& xform)
            {
                return (OrientedBox&)(obb.m_repr * xform);
            }

            Vector3 GetDimensions() const { return m_repr.GetX() + m_repr.GetY() + m_repr.GetZ(); }
            Vector3 GetCenter() const { return m_repr.GetTranslation() + GetDimensions() * 0.5f; }

        private:
            AffineTransform m_repr;
        };

        INLINE OrientedBox operator* (const OrientedBox& obb, const UniformTransform& xform)
        {
            return obb * AffineTransform(xform);
        }

        INLINE OrientedBox operator* (const AxisAlignedBox& aabb, const UniformTransform& xform)
        {
            return OrientedBox(aabb) * AffineTransform(xform);
        }

    } // namespace Math
}