#pragma once


#include "Matrix3.h"
#include "BoundingSphere.h"

using namespace GlareEngine::Math;
namespace GlareEngine
{
    namespace Math
    {
        // This transform strictly prohibits non-uniform scale.  Scale itself is barely tolerated.
        class OrthogonalTransform
        {
        public:
            INLINE OrthogonalTransform() : m_rotation(eIdentity), m_translation(eZero) {}
            INLINE OrthogonalTransform(Quaternion rotate) : m_rotation(rotate), m_translation(eZero) {}
            INLINE OrthogonalTransform(Vector3 translate) : m_rotation(eIdentity), m_translation(translate) {}
            INLINE OrthogonalTransform(Quaternion rotate, Vector3 translate) : m_rotation(rotate), m_translation(translate) {}
            INLINE OrthogonalTransform(const Matrix3& mat) : m_rotation(mat), m_translation(eZero) {}
            INLINE OrthogonalTransform(const Matrix3& mat, Vector3 translate) : m_rotation(mat), m_translation(translate) {}
            INLINE OrthogonalTransform(EIdentityTag) : m_rotation(eIdentity), m_translation(eZero) {}
            INLINE explicit OrthogonalTransform(const XMMATRIX& mat) { *this = OrthogonalTransform(Matrix3(mat), Vector3(mat.r[3])); }

            INLINE void SetRotation(Quaternion q) { m_rotation = q; }
            INLINE void SetTranslation(Vector3 v) { m_translation = v; }

            INLINE Quaternion GetRotation() const { return m_rotation; }
            INLINE Vector3 GetTranslation() const { return m_translation; }

            static INLINE OrthogonalTransform MakeXRotation(float angle) { return OrthogonalTransform(Quaternion(Vector3(eXUnitVector), angle)); }
            static INLINE OrthogonalTransform MakeYRotation(float angle) { return OrthogonalTransform(Quaternion(Vector3(eYUnitVector), angle)); }
            static INLINE OrthogonalTransform MakeZRotation(float angle) { return OrthogonalTransform(Quaternion(Vector3(eZUnitVector), angle)); }
            static INLINE OrthogonalTransform MakeTranslation(Vector3 translate) { return OrthogonalTransform(translate); }

            friend INLINE Vector3 operator* (Vector3 vec, const OrthogonalTransform& transform) { return   vec * transform.m_rotation + transform.m_translation; }
            friend INLINE Vector4 operator* (Vector4 vec, const OrthogonalTransform& transform) {
                return
                    Vector4(SetWToZero(Vector3((XMVECTOR)vec) * transform.m_rotation)) +
                    Vector4(SetWToOne(transform.m_translation)) * vec.GetW();
            }

            friend INLINE OrthogonalTransform operator* (const OrthogonalTransform& lform, const OrthogonalTransform& rform) {
                return OrthogonalTransform(rform.m_rotation * lform.m_rotation, lform.m_translation * rform.m_rotation + rform.m_translation);
            }

            INLINE OrthogonalTransform operator~ () const {
                Quaternion invertedRotation = ~m_rotation;
                return OrthogonalTransform(invertedRotation, -m_translation * invertedRotation);
            }

        private:

            Quaternion m_rotation;
            Vector3 m_translation;
        };

        //
        // A transform that lacks rotation and has only uniform scale.
        //
        class ScaleAndTranslation
        {
        public:
            INLINE ScaleAndTranslation()
            {}
            INLINE ScaleAndTranslation(EIdentityTag)
                : m_repr(eWUnitVector) {}
            INLINE ScaleAndTranslation(float tx, float ty, float tz, float scale)
                : m_repr(tx, ty, tz, scale) {}
            INLINE ScaleAndTranslation(Vector3 translate, Scalar scale)
            {
                m_repr = Vector4(translate);
                m_repr.SetW(scale);
            }
            INLINE explicit ScaleAndTranslation(const XMVECTOR& v)
                : m_repr(v) {}

            INLINE void SetScale(Scalar s) { m_repr.SetW(s); }
            INLINE void SetTranslation(Vector3 t) { m_repr.SetXYZ(t); }

            INLINE Scalar GetScale() const { return m_repr.GetW(); }
            INLINE Vector3 GetTranslation() const { return (Vector3)m_repr; }

            /*INLINE BoundSphere operator*(const BoundSphere& sphere) const
            {
                Vector4 scaledSphere = (Vector4)sphere * GetScale();
                Vector4 translation = Vector4(SetWToZero(m_repr));
                return BoundSphere(scaledSphere + translation);
            }*/

        private:
            Vector4 m_repr;
        };

        //
        // This transform allows for rotation, translation, and uniform scale
        // 
        class UniformTransform
        {
        public:
            INLINE UniformTransform()
            {}
            INLINE UniformTransform(EIdentityTag)
                : m_rotation(eIdentity), m_translationScale(eIdentity) {}
            INLINE UniformTransform(Quaternion rotation, ScaleAndTranslation transScale)
                : m_rotation(rotation), m_translationScale(transScale)
            {}
            INLINE UniformTransform(Quaternion rotation, Scalar scale, Vector3 translation)
                : m_rotation(rotation), m_translationScale(translation, scale)
            {}

            INLINE void SetRotation(Quaternion r) { m_rotation = r; }
            INLINE void SetScale(Scalar s) { m_translationScale.SetScale(s); }
            INLINE void SetTranslation(Vector3 t) { m_translationScale.SetTranslation(t); }


            INLINE Quaternion GetRotation() const { return m_rotation; }
            INLINE Scalar GetScale() const { return m_translationScale.GetScale(); }
            INLINE Vector3 GetTranslation() const { return m_translationScale.GetTranslation(); }


            friend INLINE Vector3 operator*(Vector3 vec, const UniformTransform& transpose)
            {
                return (vec * transpose.m_translationScale.GetScale()) * transpose.m_rotation + transpose.m_translationScale.GetTranslation();
            }

            //INLINE BoundSphere operator*(BoundSphere sphere) const
            //{
            //    return BoundSphere(*this * sphere.GetCenter(), GetScale() * sphere.GetRadius());
            //}

        private:
            Quaternion m_rotation;
            ScaleAndTranslation m_translationScale;
        };

        // A AffineTransform is a 3x4 matrix with an implicit 4th row = [0,0,0,1].  This is used to perform a change of
        // basis on 3D points.  An affine transformation does not have to have orthonormal basis vectors.
        class AffineTransform
        {
        public:
            INLINE AffineTransform()
            {}
            INLINE AffineTransform(Vector3 x, Vector3 y, Vector3 z, Vector3 w)
                : m_basis(x, y, z), m_translation(w) {}
            INLINE AffineTransform(Vector3 translate)
                : m_basis(eIdentity), m_translation(translate) {}
            INLINE AffineTransform(const Matrix3& mat, Vector3 translate = Vector3(eZero))
                : m_basis(mat), m_translation(translate) {}
            INLINE AffineTransform(Quaternion rot, Vector3 translate = Vector3(eZero))
                : m_basis(rot), m_translation(translate) {}
            INLINE AffineTransform(const OrthogonalTransform& xform)
                : m_basis(xform.GetRotation()), m_translation(xform.GetTranslation()) {}
            INLINE AffineTransform(const UniformTransform& xform)
            {
                m_basis = xform.GetScale() * Matrix3(xform.GetRotation());
                m_translation = xform.GetTranslation();
            }
            INLINE AffineTransform(EIdentityTag)
                : m_basis(eIdentity), m_translation(eZero) {}
            INLINE explicit AffineTransform(const XMMATRIX& mat)
                : m_basis(mat), m_translation(mat.r[3]) {}

            INLINE operator XMMATRIX() const { return (XMMATRIX&)*this; }

            INLINE void SetX(Vector3 x) { m_basis.SetX(x); }
            INLINE void SetY(Vector3 y) { m_basis.SetY(y); }
            INLINE void SetZ(Vector3 z) { m_basis.SetZ(z); }
            INLINE void SetTranslation(Vector3 w) { m_translation = w; }
            INLINE void SetBasis(const Matrix3& basis) { m_basis = basis; }

            INLINE Vector3 GetX() const { return m_basis.GetX(); }
            INLINE Vector3 GetY() const { return m_basis.GetY(); }
            INLINE Vector3 GetZ() const { return m_basis.GetZ(); }
            INLINE Vector3 GetTranslation() const { return m_translation; }
            INLINE const Matrix3& GetBasis() const { return (const Matrix3&)*this; }

            static INLINE AffineTransform MakeXRotation(float angle) { return AffineTransform(Matrix3::MakeXRotation(angle)); }
            static INLINE AffineTransform MakeYRotation(float angle) { return AffineTransform(Matrix3::MakeYRotation(angle)); }
            static INLINE AffineTransform MakeZRotation(float angle) { return AffineTransform(Matrix3::MakeZRotation(angle)); }
            static INLINE AffineTransform MakeScale(float scale) { return AffineTransform(Matrix3::MakeScale(scale)); }
            static INLINE AffineTransform MakeScale(Vector3 scale) { return AffineTransform(Matrix3::MakeScale(scale)); }
            static INLINE AffineTransform MakeTranslation(Vector3 translate) { return AffineTransform(translate); }

            INLINE Vector3 operator* (Vector3 vec) const { return vec * m_basis + m_translation; }
            INLINE AffineTransform operator* (const AffineTransform& mat) const {
                return AffineTransform(m_basis * mat.m_basis, *this * mat.GetTranslation());
            }

        private:
            Matrix3 m_basis;
            Vector3 m_translation;
        };
    }
}