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
#include "ConstantBuffer.h"

using namespace GlareEngine;

enum class ObjectType :int
{
	None,
	Sky,
	Model,
	Shadow,
	Terrain,
	PostProcessing,
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
	bool GetShadowFlag()const { return mShouldCastShadow; }

	void SetShadowRenderFlag(bool IsShadowRender) { mShouldShadowRender = IsShadowRender; }
	bool GetShadowRenderFlag()const { return mShouldShadowRender; }

	void SetVisible(bool isVisible) { mIsVisible = isVisible; }
	bool GetVisible()const { return mIsVisible; }

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) = 0;
	virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr) = 0;
	virtual void Update(float DeltaTime) = 0;
	virtual void DrawUI() = 0;

	static void BuildPSO(const PSOCommonProperty CommonProperty) {};
public:
	ObjectType mObjectType = ObjectType::None;
protected:
	//Name
	wstring mName;
	bool mShouldCastShadow = false;
	bool mShouldShadowRender = false;
	bool mIsVisible = true;
};


