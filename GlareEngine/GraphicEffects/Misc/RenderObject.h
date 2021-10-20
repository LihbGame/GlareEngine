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
	virtual void Draw(GraphicsContext& Context) = 0;
	static void BuildPSO(const RootSignature& rootSignature) {};
protected:
	//PSO
	static GraphicsPSO mPSO;
	//Name
	wstring mName;
};


