#pragma once
#include "EngineUtility.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "ModelMesh.h"

using namespace GlareEngine;

struct InstanceRenderConstants
{
	DirectX::XMFLOAT4X4			mWorldTransform = MathHelper::Identity4x4();
	int							mMaterialIndex;
};

//For Instance Draw
struct InstanceRenderData
{
	vector<vector<InstanceRenderConstants>>		mInstanceConstants;
	const ModelRenderData*						mModelData = nullptr;
};

class RanderObject
{
public:
	RanderObject() {}
	~RanderObject() {}
	
	virtual void Draw(GraphicsContext& Context) = 0;
	virtual void BuildPSO(const RootSignature& rootSignature) = 0;
};

