#pragma once
#include "Engine/EngineUtility.h"
#include "Graphics/RootSignature.h"
#include "Graphics/CommandContext.h"
#include "Animation/ModelMesh.h"
#include "Graphics/GraphicsCore.h"
#include "Graphics/CommandContext.h"
#include "Graphics/BufferManager.h"
#include "Engine/Vertex.h"
#include "Engine/InputLayout.h"
#include "Graphics/BufferManager.h"
#include "ConstantBuffer.h"
#include "Graphics/Render.h"

using namespace GlareEngine;

enum class ObjectType :int
{
	None,
	Sky,
	Model,
	Shadow,
	Terrain,
	ScreenProcessing,
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

	void SetMaskFlag(bool IsMaskMaterial) { mIsMaskMaterial = IsMaskMaterial; }
	bool GetMaskFlag()const { return mIsMaskMaterial; }

	void SetVisible(bool isVisible) { mIsVisible = isVisible; }
	bool GetVisible()const { return mIsVisible; }

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) = 0;
	virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr) = 0;
	virtual void Update(float DeltaTime, GraphicsContext* Context = nullptr) = 0;
	virtual void DrawUI() = 0;

	static void BuildPSO(const PSOCommonProperty CommonProperty) {};

#if USE_RUNTIME_PSO
	static void InitRuntimePSO() {};
#endif

public:
	ObjectType mObjectType = ObjectType::None;
protected:
	//Name
	wstring mName;
	bool mShouldCastShadow = false;
	bool mShouldShadowRender = false;
	bool mIsVisible = true;
	bool mIsMaskMaterial = false;
};


