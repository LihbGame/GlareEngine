#pragma once

#include "BoundingPlane.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"

namespace GlareEngine
{
	namespace Math
	{
		class Frustum
		{
		public:
			Frustum() {}

			Frustum(const Matrix4& ProjectionMatrix);

			enum CornerID
			{
				eNearLowerLeft, eNearUpperLeft, eNearLowerRight, eNearUpperRight,
				eFarLowerLeft,  eFarUpperLeft,  eFarLowerRight,  eFarUpperRight
			};

			enum PlaneID
			{
				eNearPlane, eFarPlane, eLeftPlane, eRightPlane, eTopPlane, eBottomPlane
			};

			Vector3         GetFrustumCorner(CornerID id) const { return m_FrustumCorners[id]; }
			BoundingPlane   GetFrustumPlane(PlaneID id) const { return m_FrustumPlanes[id]; }

			// Test whether the bounding sphere intersects the frustum.  Intersection is defined as either being
			// fully contained in the frustum, or by intersecting one or more of the planes.
			bool IntersectSphere(BoundingSphere sphere) const;

			bool IntersectBoundingBox(const AxisAlignedBox& aabb) const;

			friend inline Frustum  operator* (const Frustum& frustum, const OrthogonalTransform& xform);	// Fast
			friend inline Frustum  operator* (const Frustum& frustum, const AffineTransform& xform);		// Slow
			friend inline Frustum  operator* (const Frustum& frustum, const Matrix4& xform);				// Slowest (and most general)

		private:

			// Perspective frustum constructor 
			void ConstructPerspectiveFrustum(float HTan, float VTan, float NearClip, float FarClip);

			// Orthographic frustum constructor
			void ConstructOrthographicFrustum(float Left, float Right, float Top, float Bottom, float NearClip, float FarClip);

			Vector3 m_FrustumCorners[8];		// the corners of the frustum
			BoundingPlane m_FrustumPlanes[6];			// the bounding planes
		};

		//=======================================================================================================
		// Inline implementations
		//

		inline bool Frustum::IntersectSphere(BoundingSphere sphere) const
		{
			float radius = sphere.GetRadius();
			for (int i = 0; i < 6; ++i)
			{
				if (m_FrustumPlanes[i].DistanceFromPoint(sphere.GetCenter()) + radius < 0.0f)
					return false;
			}
			return true;
		}

		inline bool Frustum::IntersectBoundingBox(const AxisAlignedBox& aabb) const
		{
			for (int i = 0; i < 6; ++i)
			{
				BoundingPlane p = m_FrustumPlanes[i];
				Vector3 farCorner = Select(aabb.GetMin(), aabb.GetMax(), p.GetNormal() > Vector3(eZero));
				if (p.DistanceFromPoint(farCorner) < 0.0f)
					return false;
			}

			return true;
		}

		inline Frustum operator* (const Frustum& frustum, const OrthogonalTransform& xform)
		{
			Frustum result;

			for (int i = 0; i < 8; ++i)
				result.m_FrustumCorners[i] = frustum.m_FrustumCorners[i] * xform;

			for (int i = 0; i < 6; ++i)
				result.m_FrustumPlanes[i] = frustum.m_FrustumPlanes[i] * xform;

			return result;
		}

		inline Frustum operator* (const Frustum& frustum, const AffineTransform& xform)
		{
			Frustum result;

			for (int i = 0; i < 8; ++i)
				result.m_FrustumCorners[i] = xform * frustum.m_FrustumCorners[i];

			Matrix4 XForm = Transpose(Invert(Matrix4(xform)));

			for (int i = 0; i < 6; ++i)
				result.m_FrustumPlanes[i] = BoundingPlane(Vector4(frustum.m_FrustumPlanes[i]) * XForm);

			return result;
		}

		inline Frustum operator* (const Frustum& frustum, const Matrix4& xform)
		{
			Frustum result;

			for (int i = 0; i < 8; ++i)
				result.m_FrustumCorners[i] = Vector3(frustum.m_FrustumCorners[i] * xform);

			Matrix4 XForm = Transpose(Invert(xform));

			for (int i = 0; i < 6; ++i)
				result.m_FrustumPlanes[i] = BoundingPlane(Vector4(frustum.m_FrustumPlanes[i]) * XForm);

			return result;
		}

	} // namespace Math
}

