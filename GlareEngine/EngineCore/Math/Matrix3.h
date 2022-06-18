#pragma once

#include "Quaternion.h"
namespace GlareEngine
{
    namespace Math
    {
        // Represents a 3x3 matrix while occuping a 3x4 memory footprint.  The unused row and column are undefined but implicitly
        // (0, 0, 0, 1).  Constructing a Matrix4 will make those values explicit.
        __declspec(align(16)) class Matrix3
        {
        public:
            INLINE Matrix3() {}
            INLINE Matrix3(Vector3 x, Vector3 y, Vector3 z) { m_mat[0] = x; m_mat[1] = y; m_mat[2] = z; }
            INLINE Matrix3(const Matrix3& m) { m_mat[0] = m.m_mat[0]; m_mat[1] = m.m_mat[1]; m_mat[2] = m.m_mat[2]; }
            INLINE Matrix3(Quaternion q) { *this = Matrix3(XMMatrixRotationQuaternion(q)); }
            INLINE explicit Matrix3(const XMMATRIX& m) { m_mat[0] = Vector3(m.r[0]); m_mat[1] = Vector3(m.r[1]); m_mat[2] = Vector3(m.r[2]); }
            INLINE explicit Matrix3(EIdentityTag) { m_mat[0] = Vector3(eXUnitVector); m_mat[1] = Vector3(eYUnitVector); m_mat[2] = Vector3(eZUnitVector); }
            INLINE explicit Matrix3(EZeroTag) { m_mat[0] = m_mat[1] = m_mat[2] = Vector3(eZero); }

            INLINE void SetX(Vector3 x) { m_mat[0] = x; }
            INLINE void SetY(Vector3 y) { m_mat[1] = y; }
            INLINE void SetZ(Vector3 z) { m_mat[2] = z; }

            INLINE Vector3 GetX() const { return m_mat[0]; }
            INLINE Vector3 GetY() const { return m_mat[1]; }
            INLINE Vector3 GetZ() const { return m_mat[2]; }

            static INLINE Matrix3 MakeXRotation(float angle) { return Matrix3(XMMatrixRotationX(angle)); }
            static INLINE Matrix3 MakeYRotation(float angle) { return Matrix3(XMMatrixRotationY(angle)); }
            static INLINE Matrix3 MakeZRotation(float angle) { return Matrix3(XMMatrixRotationZ(angle)); }
            static INLINE Matrix3 MakeScale(float scale) { return Matrix3(XMMatrixScaling(scale, scale, scale)); }
            static INLINE Matrix3 MakeScale(float sx, float sy, float sz) { return Matrix3(XMMatrixScaling(sx, sy, sz)); }
            static INLINE Matrix3 MakeScale(const XMFLOAT3& scale) { return Matrix3(XMMatrixScaling(scale.x, scale.y, scale.z)); }
            static INLINE Matrix3 MakeScale(Vector3 scale) { return Matrix3(XMMatrixScalingFromVector(scale)); }

            // Useful for DirectXMath interaction.  WARNING:  Only the 3x3 elements are defined.
            INLINE operator XMMATRIX() const { return XMMATRIX(m_mat[0], m_mat[1], m_mat[2], XMVectorZero()); }

            friend INLINE Matrix3 operator* (Scalar scl, const Matrix3& mat) { return Matrix3(scl * mat.GetX(), scl * mat.GetY(), scl * mat.GetZ()); }
            friend INLINE Vector3 operator* (Vector3 vec, const Matrix3& mat) { return Vector3(XMVector3TransformNormal(vec, mat)); }
            friend INLINE Matrix3 operator* (const Matrix3& lmat, const Matrix3& rmat) { return Matrix3(lmat.GetX() * rmat, lmat.GetY() * rmat, lmat.GetZ() * rmat); }

        private:
            Vector3 m_mat[3];
        };

    } // namespace Math
}