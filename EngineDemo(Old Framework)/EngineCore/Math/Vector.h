#pragma once

#include "Scalar.h"

namespace GlareEngine
{
	namespace Math
	{
		class Vector4;

		//具有未指定的第四分量的3向量。 根据上下文的不同，W可以为0或1，但是两者都是隐式的。
		//由于性能原因，未定义第四个组件的实际值。
		class Vector3
		{
		public:

			INLINE Vector3() {}
			INLINE Vector3(float x, float y, float z) { m_vec = XMVectorSet(x, y, z, z); }
			INLINE Vector3(const XMFLOAT3& v) { m_vec = XMLoadFloat3(&v); }
			INLINE Vector3(const Vector3& v) { m_vec = v; }
			INLINE Vector3(Scalar s) { m_vec = s; }
			INLINE explicit Vector3(Vector4 v);
			INLINE explicit Vector3(FXMVECTOR vec) { m_vec = vec; }
			INLINE explicit Vector3(EZeroTag) { m_vec = SplatZero(); }
			INLINE explicit Vector3(EIdentityTag) { m_vec = SplatOne(); }
			INLINE explicit Vector3(EXUnitVector) { m_vec = CreateXUnitVector(); }
			INLINE explicit Vector3(EYUnitVector) { m_vec = CreateYUnitVector(); }
			INLINE explicit Vector3(EZUnitVector) { m_vec = CreateZUnitVector(); }

			INLINE operator XMVECTOR() const { return m_vec; }

			INLINE Scalar GetX() const { return Scalar(XMVectorSplatX(m_vec)); }
			INLINE Scalar GetY() const { return Scalar(XMVectorSplatY(m_vec)); }
			INLINE Scalar GetZ() const { return Scalar(XMVectorSplatZ(m_vec)); }
			INLINE void SetX(Scalar x) { m_vec = XMVectorPermute<4, 1, 2, 3>(m_vec, x); }
			INLINE void SetY(Scalar y) { m_vec = XMVectorPermute<0, 5, 2, 3>(m_vec, y); }
			INLINE void SetZ(Scalar z) { m_vec = XMVectorPermute<0, 1, 6, 3>(m_vec, z); }

			INLINE Vector3 operator- () const { return Vector3(XMVectorNegate(m_vec)); }
			INLINE Vector3 operator+ (Vector3 v2) const { return Vector3(XMVectorAdd(m_vec, v2)); }
			INLINE Vector3 operator- (Vector3 v2) const { return Vector3(XMVectorSubtract(m_vec, v2)); }
			INLINE Vector3 operator* (Vector3 v2) const { return Vector3(XMVectorMultiply(m_vec, v2)); }
			INLINE Vector3 operator/ (Vector3 v2) const { return Vector3(XMVectorDivide(m_vec, v2)); }
			INLINE Vector3 operator* (Scalar  v2) const { return *this * Vector3(v2); }
			INLINE Vector3 operator/ (Scalar  v2) const { return *this / Vector3(v2); }
			INLINE Vector3 operator* (float  v2) const { return *this * Scalar(v2); }
			INLINE Vector3 operator/ (float  v2) const { return *this / Scalar(v2); }

			INLINE Vector3& operator += (Vector3 v) { *this = *this + v; return *this; }
			INLINE Vector3& operator -= (Vector3 v) { *this = *this - v; return *this; }
			INLINE Vector3& operator *= (Vector3 v) { *this = *this * v; return *this; }
			INLINE Vector3& operator /= (Vector3 v) { *this = *this / v; return *this; }

			INLINE friend Vector3 operator* (Scalar  v1, Vector3 v2) { return Vector3(v1) * v2; }
			INLINE friend Vector3 operator/ (Scalar  v1, Vector3 v2) { return Vector3(v1) / v2; }
			INLINE friend Vector3 operator* (float   v1, Vector3 v2) { return Scalar(v1) * v2; }
			INLINE friend Vector3 operator/ (float   v1, Vector3 v2) { return Scalar(v1) / v2; }

		protected:
			XMVECTOR m_vec;
		};

		// A 4-vector, completely defined.
		class Vector4
		{
		public:
			INLINE Vector4() {}
			INLINE Vector4(float x, float y, float z, float w) { m_vec = XMVectorSet(x, y, z, w); }
			INLINE Vector4(const XMFLOAT4& v) { m_vec = XMLoadFloat4(&v); }
			INLINE Vector4(Vector3 xyz, float w) { m_vec = XMVectorSetW(xyz, w); }
			INLINE Vector4(const Vector4& v) { m_vec = v; }
			INLINE Vector4(const Scalar& s) { m_vec = s; }
			INLINE explicit Vector4(Vector3 xyz) { m_vec = SetWToOne(xyz); }
			INLINE explicit Vector4(FXMVECTOR vec) { m_vec = vec; }
			INLINE explicit Vector4(EZeroTag) { m_vec = SplatZero(); }
			INLINE explicit Vector4(EIdentityTag) { m_vec = SplatOne(); }
			INLINE explicit Vector4(EXUnitVector) { m_vec = CreateXUnitVector(); }
			INLINE explicit Vector4(EYUnitVector) { m_vec = CreateYUnitVector(); }
			INLINE explicit Vector4(EZUnitVector) { m_vec = CreateZUnitVector(); }
			INLINE explicit Vector4(EWUnitVector) { m_vec = CreateWUnitVector(); }

			INLINE operator XMVECTOR() const { return m_vec; }

			INLINE Scalar GetX() const { return Scalar(XMVectorSplatX(m_vec)); }
			INLINE Scalar GetY() const { return Scalar(XMVectorSplatY(m_vec)); }
			INLINE Scalar GetZ() const { return Scalar(XMVectorSplatZ(m_vec)); }
			INLINE Scalar GetW() const { return Scalar(XMVectorSplatW(m_vec)); }
			INLINE void SetX(Scalar x) { m_vec = XMVectorPermute<4, 1, 2, 3>(m_vec, x); }
			INLINE void SetY(Scalar y) { m_vec = XMVectorPermute<0, 5, 2, 3>(m_vec, y); }
			INLINE void SetZ(Scalar z) { m_vec = XMVectorPermute<0, 1, 6, 3>(m_vec, z); }
			INLINE void SetW(Scalar w) { m_vec = XMVectorPermute<0, 1, 2, 7>(m_vec, w); }
			INLINE void SetXYZ(Vector3 xyz) { m_vec = XMVectorPermute<0, 1, 2, 7>(xyz, m_vec); }

			INLINE Vector4 operator- () const { return Vector4(XMVectorNegate(m_vec)); }
			INLINE Vector4 operator+ (Vector4 v2) const { return Vector4(XMVectorAdd(m_vec, v2)); }
			INLINE Vector4 operator- (Vector4 v2) const { return Vector4(XMVectorSubtract(m_vec, v2)); }
			INLINE Vector4 operator* (Vector4 v2) const { return Vector4(XMVectorMultiply(m_vec, v2)); }
			INLINE Vector4 operator/ (Vector4 v2) const { return Vector4(XMVectorDivide(m_vec, v2)); }
			INLINE Vector4 operator* (Scalar  v2) const { return *this * Vector4(v2); }
			INLINE Vector4 operator/ (Scalar  v2) const { return *this / Vector4(v2); }
			INLINE Vector4 operator* (float   v2) const { return *this * Scalar(v2); }
			INLINE Vector4 operator/ (float   v2) const { return *this / Scalar(v2); }

			INLINE void operator*= (float   v2) { *this = *this * Scalar(v2); }
			INLINE void operator/= (float   v2) { *this = *this / Scalar(v2); }

			INLINE friend Vector4 operator* (Scalar  v1, Vector4 v2) { return Vector4(v1) * v2; }
			INLINE friend Vector4 operator/ (Scalar  v1, Vector4 v2) { return Vector4(v1) / v2; }
			INLINE friend Vector4 operator* (float   v1, Vector4 v2) { return Scalar(v1) * v2; }
			INLINE friend Vector4 operator/ (float   v1, Vector4 v2) { return Scalar(v1) / v2; }

		protected:
			XMVECTOR m_vec;
		};

		INLINE Vector3::Vector3(Vector4 v)
		{
			Scalar W = v.GetW();
			m_vec = XMVectorSelect(XMVectorDivide(v, W), v, XMVectorEqual(W, SplatZero()));
		}

		class BoolVector
		{
		public:
			INLINE BoolVector(FXMVECTOR vec) { m_vec = vec; }
			INLINE operator XMVECTOR() const { return m_vec; }
		protected:
			XMVECTOR m_vec;
		};

	} // namespace Math
}//namespace GlareEngine