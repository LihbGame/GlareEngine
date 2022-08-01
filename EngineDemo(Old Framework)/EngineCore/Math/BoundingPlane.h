#pragma once

#include "VectorMath.h"

namespace GlareEngine
{
    namespace Math
    {
        class BoundingPlane
        {
        public:

            BoundingPlane() {}
            BoundingPlane(Vector3 normalToPlane, float distanceFromOrigin) : m_repr(normalToPlane, distanceFromOrigin) {}
            BoundingPlane(Vector3 pointOnPlane, Vector3 normalToPlane);
            BoundingPlane(float A, float B, float C, float D) : m_repr(A, B, C, D) {}
            BoundingPlane(const BoundingPlane& plane) : m_repr(plane.m_repr) {}
            explicit BoundingPlane(Vector4 plane) : m_repr(plane) {}

            INLINE operator Vector4() const { return m_repr; }

            // Returns the direction the plane is facing.  (Warning:  might not be normalized.)
            Vector3 GetNormal(void) const { return Vector3(XMVECTOR(m_repr)); }

            // Returns the point on the plane closest to the origin
            Vector3 GetPointOnPlane(void) const { return -GetNormal() * m_repr.GetW(); }

            // Distance from 3D point
            Scalar DistanceFromPoint(Vector3 point) const
            {
                return Dot(point, GetNormal()) + m_repr.GetW();
            }

            // Distance from homogeneous point
            Scalar DistanceFromPoint(Vector4 point) const
            {
                return Dot(point, m_repr);
            }

            // Most efficient way to transform a plane.  (Involves one quaternion-vector rotation and one dot product.)
            friend BoundingPlane operator* (BoundingPlane plane, const OrthogonalTransform& xform)
            {
                Vector3 normalToPlane = plane.GetNormal() * xform.GetRotation();
                float distanceFromOrigin = plane.m_repr.GetW() - Dot(normalToPlane, xform.GetTranslation());
                return BoundingPlane(normalToPlane, distanceFromOrigin);
            }

            // Less efficient way to transform a plane (but handles affine transformations.)
            friend BoundingPlane operator* (BoundingPlane plane, const Matrix4& mat)
            {
                return BoundingPlane(plane.m_repr * Transpose(Invert(mat)));
            }

        private:

            Vector4 m_repr;
        };

        //=======================================================================================================
        // Inline implementations
        //
        inline BoundingPlane::BoundingPlane(Vector3 pointOnPlane, Vector3 normalToPlane)
        {
            // Guarantee a normal.  This constructor isn't meant to be called frequently, but if it is, we can change this.
            normalToPlane = Normalize(normalToPlane);
            m_repr = Vector4(normalToPlane, -Dot(pointOnPlane, normalToPlane));
        }

        //=======================================================================================================
        // Functions operating on planes
        //
        inline BoundingPlane PlaneFromPointsCCW(Vector3 A, Vector3 B, Vector3 C)
        {
            return BoundingPlane(A, Cross(B - A, C - A));
        }


    } // namespace Math
}