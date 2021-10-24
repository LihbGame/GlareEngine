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

	static void BuildPSO(const PSOCommonProperty CommonProperty);
private:
	//PSO
	static GraphicsPSO mPSO;

	InstanceRenderData mInstanceData;
};

