#pragma once
#include "EngineUtility.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "ModelMesh.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "Vertex.h"
#include "InputLayout.h"
#include "BufferManager.h"

using namespace GlareEngine;

struct InstanceRenderConstants
{
	DirectX::XMFLOAT4X4			mWorldTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4			mTexTransform = MathHelper::Identity4x4();
	int							mMaterialIndex;
	int							mPAD1;
	int							mPAD2;
	int							mPAD3;
};

enum class PSOType :int
{
	Default,
	Shadow,
	Count
};

//For Instance Draw
struct InstanceRenderData
{
	vector<vector<InstanceRenderConstants>>		mInstanceConstants;
	const ModelRenderData*							mModelData = nullptr;
};

class RenderObject
{
public:
	RenderObject() {}
	~RenderObject() {}
public:
	void SetName(wstring name) { mName = name; }
	wstring GetName() const { return mName; }
	void SetShadowFlag(bool IsCastShadow) { mShouldCastShadow = IsCastShadow; }
	bool GetShadowFlag() { return mShouldCastShadow; }

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) = 0;
	static void BuildPSO(const RootSignature& rootSignature) {};
protected:
	//Name
	wstring mName;
	bool mShouldCastShadow = false;
};


