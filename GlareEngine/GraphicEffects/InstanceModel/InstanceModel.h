#pragma once

#include "Engine/EngineUtility.h"
#include "Engine/GeometryGenerator.h"
#include "Engine/Vertex.h"
#include "Misc/RenderObject.h"

class InstanceModel 
	:public RenderObject
{
public:
	InstanceModel(wstring Name, InstanceRenderData InstanceData);
	~InstanceModel() {};

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

	virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr);

	virtual void DrawUI() {}

	virtual void InitMaterial();

	static void BuildPSO(const PSOCommonProperty CommonProperty);

#if USE_RUNTIME_PSO
	static void InitRuntimePSO();
#endif

	void Update(float dt, GraphicsContext* Context = nullptr) {}
private:
	//PSO
	static GraphicsPSO mPSO;

	InstanceRenderData mInstanceData;
};

