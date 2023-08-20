#include "Camera.h"
#include "Math/VectorMath.h"
#include "Shadow/ShadowMap.h"

using namespace DirectX;
using namespace GlareEngine::Math;

Camera::Camera(bool isReverseZ, bool isInfiniteZ):
mIsReverseZ(isReverseZ),
mIsInfiniteZ(isInfiniteZ),
mPosition { 0.0f, 0.0f, 0.0f },
mRight{ 1.0f, 0.0f, 0.0f },
mUp{ 0.0f, 1.0f, 0.0f },
mLook{ 0.0f, 0.0f, 1.0f }
{
	SetLens(0.25f * MathHelper::Pi, 16.0f / 9.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

XMVECTOR Camera::GetPosition()const
{
	return XMLoadFloat3(&mPosition);
}

XMFLOAT3 Camera::GetPosition3f()const
{
	return mPosition;
}

void Camera::SetPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
	mViewDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
	mPosition = v;
	mViewDirty = true;
}

XMVECTOR Camera::GetRight()const
{
	return XMLoadFloat3(&mRight);
}

XMFLOAT3 Camera::GetRight3f()const
{
	return mRight;
}

XMVECTOR Camera::GetUp()const
{
	return XMLoadFloat3(&mUp);
}

XMFLOAT3 Camera::GetUp3f()const
{
	return mUp;
}

XMVECTOR Camera::GetLook()const
{
	return XMLoadFloat3(&mLook);
}

XMFLOAT3 Camera::GetLook3f()const
{
	return mLook;
}

float Camera::GetNearZ()const
{
	return mNearZ;
}

float Camera::GetFarZ()const
{
	return mFarZ;
}

float Camera::GetAspect()const
{
	return mAspect;
}

float Camera::GetFovY()const
{
	return mFovY;
}

float Camera::GetFovX()const
{
	float halfWidth = 0.5f*GetNearWindowWidth();
	return 2.0f*atan(halfWidth / mNearZ);
}

float Camera::GetNearWindowWidth()const
{
	return mAspect * mNearWindowHeight;
}

float Camera::GetNearWindowHeight()const
{
	return mNearWindowHeight;
}

float Camera::GetFarWindowWidth()const
{
	return mAspect * mFarWindowHeight;
}

float Camera::GetFarWindowHeight()const
{
	return mFarWindowHeight;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	// cache properties
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf( 0.5f*mFovY );
	mFarWindowHeight  = 2.0f * mFarZ * tanf( 0.5f*mFovY );

	float Y = 1.0f / std::tanf(mFovY * 0.5f);
	float X = Y / mAspect;

	float Q1, Q2;

	// ReverseZ puts far plane at Z=0 and near plane at Z=1.  This is never a bad idea, and it's
	// actually a great idea with F32 depth buffers to redistribute precision more evenly across
	// the entire range. It requires clearing Z to 0.0f and using a GREATER variant depth test.
	// Some care must also be done to properly reconstruct linear W in a pixel shader from hyperbolic Z.
	if (mIsReverseZ)
	{
		if (mIsInfiniteZ)
		{
			Q1 = 0.0f;
			Q2 = mNearZ;
		}
		else
		{
			Q1 = mNearZ / (mNearZ - mFarZ);
			Q2 = -Q1 * mFarZ;
		}
	}
	else
	{
		if (mIsInfiniteZ)
		{
			Q1 = 1.0f;
			Q2 = -mNearZ;
		}
		else
		{
			Q1 = mFarZ / (mFarZ - mNearZ);
			Q2 = -Q1 * mNearZ;
		}
	}

	XMMATRIX proj = Matrix4(
		Vector4(X, 0.0f, 0.0f, 0.0f),
		Vector4(0.0f, Y, 0.0f, 0.0f),
		Vector4(0.0f, 0.0f, Q1, 1.0f),
		Vector4(0.0f, 0.0f, Q2, 0.0f)
	);
	
	XMStoreFloat4x4(&mProj, proj);

	//create view space frustum
	m_FrustumVS = Frustum(Matrix4(proj));

}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);

	mViewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR U = XMLoadFloat3(&up);

	LookAt(P, T, U);

	mViewDirty = true;
}

XMMATRIX Camera::GetView()const
{
	assert(!mViewDirty);
	return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj()const
{
	return XMLoadFloat4x4(&mProj);
}

DirectX::XMFLOAT4X4 Camera::GetViewProj() const
{
	return mViewProj;
}

DirectX::XMMATRIX Camera::GetViewProjection() const
{
	return XMLoadFloat4x4(&mViewProj);
}

void Camera::SetView(XMMATRIX view)
{
	XMStoreFloat4x4(&mView, view);
}

XMFLOAT4X4 Camera::GetView4x4f()const
{
	assert(!mViewDirty);
	return mView;
}

XMFLOAT4X4 Camera::GetProj4x4f()const
{
	return mProj;
}

void Camera::Strafe(float d)
{
	// mPosition += d*mRight
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&mRight);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

	mViewDirty = true;
}

void Camera::Walk(float d)
{
	// mPosition += d*mLook
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&mLook);
	XMVECTOR p = XMLoadFloat3(&mPosition);
	XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

	mViewDirty = true;
}

void Camera::Up(float d)
{
	mPosition.y += d;
	mViewDirty = true;
}

void Camera::Down(float d)
{
	mPosition.y -= d;
	mViewDirty = true;
}

void Camera::Pitch(float angle)
{
	// Rotate up and look vector about the right vector.

	XMMATRIX R = DirectX::XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp,   XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::RotateY(float angle)
{
	// Rotate the basis vectors about the world y-axis.

	XMFLOAT3 up = XMFLOAT3(0, 1, 0);
	XMStoreFloat3(&up, DirectX::XMVector3Dot(XMLoadFloat3(&mUp), XMLoadFloat3(&up)));
	if (up.x < 0.0f)
	{
		angle = -angle;
	}

	XMMATRIX R = DirectX::XMMatrixRotationY(angle);
	
	XMStoreFloat3(&mRight,   XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
	m_PreviousViewProjMatrix = Matrix4(mViewProjNoTranspose);
	if(mViewDirty)
	{
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPosition);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		mView(0, 0) = mRight.x;
		mView(1, 0) = mRight.y;
		mView(2, 0) = mRight.z;
		mView(3, 0) = x;

		mView(0, 1) = mUp.x;
		mView(1, 1) = mUp.y;
		mView(2, 1) = mUp.z;
		mView(3, 1) = y;

		mView(0, 2) = mLook.x;
		mView(1, 2) = mLook.y;
		mView(2, 2) = mLook.z;
		mView(3, 2) = z;

		mView(0, 3) = 0.0f;
		mView(1, 3) = 0.0f;
		mView(2, 3) = 0.0f;
		mView(3, 3) = 1.0f;

		mViewDirty = false;
		mViewChanged = true;
		
		XMVECTOR  MatrixDeterminant = DirectX::XMMatrixDeterminant(GetView());
		mCameraToWorld = Matrix4(DirectX::XMMatrixInverse(&MatrixDeterminant, GetView()));

		mViewProjNoTranspose = GetView() * GetProj();
		XMStoreFloat4x4(&mViewProj, DirectX::XMMatrixTranspose(mViewProjNoTranspose));

		//create world space frustum
		m_FrustumWS = m_FrustumVS * mCameraToWorld;
	}
	else
	{
		mViewChanged = false;
	}

	m_ReprojectMatrix = Invert(Matrix4(mViewProjNoTranspose)) * m_PreviousViewProjMatrix;
}

XMMATRIX ShadowCamera::GetView() const
{
	XMFLOAT4X4 View = m_pShadowMap->GetView();
	return  XMLoadFloat4x4(&View);
}

const Frustum& ShadowCamera::GetViewSpaceFrustum()
{
	assert(m_pShadowMap);
	//create view space frustum
	m_FrustumVS = Frustum(Matrix4(m_pShadowMap->GetProj()));
	return m_FrustumVS;
}