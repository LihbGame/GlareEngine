
#ifndef CAMERA_H
#define CAMERA_H

#include "EngineUtility.h"
#include "Frustum.h"


class Camera
{
public:

	Camera(bool isReverseZ = false, bool isInfiniteZ = false);
	~Camera();

	//��ȡ/�����������λ�á�
	DirectX::XMVECTOR GetPosition()const;
	DirectX::XMFLOAT3 GetPosition3f()const;

	void ResetPosition();
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& v);
	
	//��ȡ�������������
	DirectX::XMVECTOR GetRight()const;
	DirectX::XMFLOAT3 GetRight3f()const;
	DirectX::XMVECTOR GetUp()const;
	DirectX::XMFLOAT3 GetUp3f()const;
	DirectX::XMVECTOR GetLook()const;
	DirectX::XMFLOAT3 GetLook3f()const;

	//��ȡ��׶���ԡ�
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;
	
	//������׶��
	void SetLens(float fovY, float aspect, float zn, float zf);
	//���� 
	void FocalLength(float nz);
	//���ð뾶(�뾶������������ֵ)��
	void SetRadius(float r);

	//ͨ��LookAt������������ռ䡣
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

	//��ȡView/Proj����
	DirectX::XMMATRIX GetView()const;
	DirectX::XMMATRIX GetProj()const;

	void SetView(XMMATRIX view);

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// ����ƶ�
	void Move(float x, float y, float z);

	// ��ת���
	void Pitch(float angle);
	void RotateY(float angle);

	// �������λ��
	void UpdatePosition();
	//�޸������λ��/����󣬵��ø�����ͼ����
	void UpdateViewMatrix();

private:
	float Clamp(float x, float low, float high)
	{
		return x < low ? low : (x > high ? high : x);
	}

private:
	float Theta = 1.5f * XM_PI;
	float Pi = XM_PIDIV4;

	bool mIsReverseZ;
	bool mIsInfiniteZ;

	//�������ϵ�����������������ռ䡣
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, -2.0f };// �����
	DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	// Cache frustum properties.
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float Aspect = 0.0f;
	float FovY = 0.0f;
	float Radius = 10.0f;

	bool mViewDirty = true;

	//������ͼ/�������
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();


	Matrix4 mCameraToWorld;
};

#endif // CAMERA_H