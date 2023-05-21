
#ifndef CAMERA_H
#define CAMERA_H

#include "EngineUtility.h"
#include "Math/Frustum.h"

class ShadowMap;

class Camera
{
public:

	Camera(bool isReverseZ = false, bool isInfiniteZ = false);
	~Camera();

	// Get/Set world camera position.
	DirectX::XMVECTOR GetPosition()const;
	DirectX::XMFLOAT3 GetPosition3f()const;
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& v);
	
	// Get camera basis vectors.
	DirectX::XMVECTOR GetRight()const;
	DirectX::XMFLOAT3 GetRight3f()const;
	DirectX::XMVECTOR GetUp()const;
	DirectX::XMFLOAT3 GetUp3f()const;
	DirectX::XMVECTOR GetLook()const;
	DirectX::XMFLOAT3 GetLook3f()const;

	// Get frustum properties.
	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const;
	float GetFovX()const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;
	
	// Set frustum.
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters.
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

	virtual const Frustum& GetViewSpaceFrustum()  { return m_FrustumVS; }
	virtual const Frustum& GetWorldSpaceFrustum() const { return m_FrustumWS; }


	// Get View/Proj matrices.
	virtual DirectX::XMMATRIX GetView()const;
	virtual DirectX::XMMATRIX GetProj()const;
	virtual DirectX::XMFLOAT4X4 GetViewProj()const;
	virtual DirectX::XMMATRIX GetViewProjection()const;

	void SetView(XMMATRIX view);

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	void Up(float d);
	void Down(float d);

	// Rotate the camera.
	void Pitch(float angle);
	void RotateY(float angle);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

	bool GetViewChange() const { return mViewChanged; }
protected:
	virtual ShadowMap* GetShadowMap() { return nullptr; }

	bool mIsReverseZ;
	bool mIsInfiniteZ;

	// Camera coordinate system with coordinates relative to world space.
	DirectX::XMFLOAT3 mPosition			= { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRight			= { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp				= { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook				= { 0.0f, 0.0f, 1.0f };

	// Cache frustum properties.
	float mNearZ						= 0.0f;
	float mFarZ							= 0.0f;
	float mAspect						= 0.0f;
	float mFovY							= 0.0f;
	float mNearWindowHeight				= 0.0f;
	float mFarWindowHeight				= 0.0f;

	bool mViewDirty						= true;
	bool mViewChanged					= true;

	// Cache View/Proj matrices.
	DirectX::XMFLOAT4X4 mView			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj			= MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mViewProj		= MathHelper::Identity4x4();

	Matrix4 mCameraToWorld;
	//Frustum
	Frustum m_FrustumVS;
	Frustum m_FrustumWS;
};


class ShadowCamera : public Camera
{
public:

	ShadowCamera(ShadowMap* shadowMap) :m_pShadowMap(shadowMap) {}

	virtual XMMATRIX GetView() const;

	virtual const Frustum& GetViewSpaceFrustum();

	virtual const Frustum& GetWorldSpaceFrustum() const { return m_FrustumWS; }

	virtual ShadowMap* GetShadowMap() { return m_pShadowMap; }

private:
	ShadowMap* m_pShadowMap = nullptr;
};


#endif // CAMERA_H