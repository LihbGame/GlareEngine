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

	virtual void Draw(GraphicsContext& Context);

	static void BuildPSO(const RootSignature& rootSignature);

private:
	//PSO
	static GraphicsPSO mPSO;

	InstanceRenderData mInstanceData;
};

