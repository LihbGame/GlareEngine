#pragma once

#include "EngineUtility.h"
#include "GeometryGenerator.h"
#include "Vertex.h"
#include "RenderObject.h"

class InstanceModel 
	:public RenderObject
{
public:
	InstanceModel(wstring Name, InstanceRenderData InstanceData);
	~InstanceModel() {};

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr);

	virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr);

	virtual void DrawUI() {}

	static void BuildPSO(const PSOCommonProperty CommonProperty);

	void Update(float dt) {}
private:
	//PSO
	static GraphicsPSO mPSO;

	InstanceRenderData mInstanceData;
};

