
#ifndef CAMERA_H
#define CAMERA_H

#include "EngineUtility.h"
#include "Frustum.h"


class Camera
{
public:

	Camera(bool isReverseZ = false, bool isInfiniteZ = false);
	~Camera();

	//获取/设置世界相机位置。
	DirectX::XMVECTOR GetPosition()const;
	DirectX::XMFLOAT3 GetPosition3f()const;
	
	void ResetPosition();
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& v);
	
	//获取相机基本向量。
	DirectX::XMVECTOR GetRight()const;
	DirectX::XMFLOAT3 GetRight3f()const;
	DirectX::XMVECTOR GetUp()const;
	DirectX::XMFLOAT3 GetUp3f()const;
	DirectX::XMVECTOR GetLook()const;
	DirectX::XMFLOAT3 GetLook3f()const;

	//获取视锥属性。
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	//设置视锥。
	void SetLens(float fovY, float aspect, float zn, float zf);
	//焦距 
	void FocalLength(float nz);
	//设置半径(半径会加上所输入的值)。
	void SetRadius(float r);

	//通过LookAt参数定义相机空间。
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);


	//获取View/Proj矩阵。
	DirectX::XMMATRIX GetView()const;
	DirectX::XMMATRIX GetProj()const;

	void SetView(XMMATRIX view);

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// 相机移动
	void Move(float x, float y, float z);

	// 旋转相机
	void Pitch(float angle);
	void RotateY(float angle);

	// 更新相机位置
	void UpdatePosition();
	//修改相机的位置/方向后，调用更新视图矩阵。
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

	//相机坐标系，其坐标相对于世界空间。
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };// 相机点
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

	//缓存视图/对象矩阵。
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();


	Matrix4 mCameraToWorld;
};

#endif // CAMERA_H