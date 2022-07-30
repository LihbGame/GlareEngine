#include "Camera.h"
#include "VectorMath.h"

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
	SetLens(0.25f*MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
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

void Camera::ResetPosition()
{
	//将球面坐标转换为笛卡尔坐标。
	float posx = Radius * sinf(Pi) * cosf(Theta) / 2;
	float posy = Radius * cosf(Pi) / 2;
	float posz = Radius * sinf(Pi) * sinf(Theta) / 2;

	mPosition = XMFLOAT3(posx, posy, posz);
	mViewDirty = true;
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
	return NearZ;
}

float Camera::GetFarZ()const
{
	return FarZ;
}

float Camera::GetAspect()const
{
	return Aspect;
}

float Camera::GetFovY()const
{
	return FovY;
}

float Camera::GetFovX()const
{
	float halfWidth = 0.5f;
	return 2.0f*atan(halfWidth / NearZ);
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
	//缓存属性
	FovY = fovY;
	Aspect = aspect;
	NearZ = zn;
	FarZ = zf;

	XMMATRIX P = XMMatrixPerspectiveFovLH(FovY, Aspect, NearZ, FarZ);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::FocalLength(float nz)
{
	FovY += nz;
	FovY = Clamp(FovY, 0.01f, 3.0f);

	XMMATRIX P = XMMatrixPerspectiveFovLH(FovY, Aspect, NearZ, FarZ);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::SetRadius(float r)
{
	Radius = r;

	mViewDirty = true;
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

void Camera::Move(float x, float y, float z)
{
	{
		XMVECTOR s = XMVectorReplicate(x);
		XMVECTOR r = XMLoadFloat3(&mRight);
		XMVECTOR p = XMLoadFloat3(&mPosition);
		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
	}
	{
		XMVECTOR s = XMVectorReplicate(-y);
		XMVECTOR r = XMLoadFloat3(&mUp);
		XMVECTOR p = XMLoadFloat3(&mPosition);
		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
	}
	{
		XMVECTOR s = XMVectorReplicate(z);
		XMVECTOR l = XMLoadFloat3(&mLook);
		XMVECTOR p = XMLoadFloat3(&mPosition);
		XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));
	}

	mViewDirty = true;
}

void Camera::Pitch(float angle)
{
	// Rotate up and look vector about the right vector.

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

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

	XMMATRIX R = XMMatrixRotationY(angle);
	
	XMStoreFloat3(&mRight,   XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::UpdatePosition()
{
	//将球面坐标转换为笛卡尔坐标。
	float posx = Radius * sinf(Pi) * cosf(Theta) / 2 + mPosition.x;
	float posy = Radius * cosf(Pi) / 2 + mPosition.y;
	float posz = Radius * sinf(Pi) * sinf(Theta) / 2 + mPosition.z;

	mPosition = XMFLOAT3(posx, posy, posz);
}

void Camera::UpdateViewMatrix()
{
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

		
		XMVECTOR Determinant = XMMatrixDeterminant(GetView());
		mCameraToWorld = Matrix4(XMMatrixInverse(&Determinant, GetView()));
	}
}


