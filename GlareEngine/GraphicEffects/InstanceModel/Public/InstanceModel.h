#pragma once

#include "EngineUtility.h"
#include "GeometryGenerator.h"
#include "Vertex.h"
#include "RanderObject.h"

class InstanceModel 
	:public RanderObject
{
public:
	InstanceModel(InstanceRenderData InstanceData);
	~InstanceModel() {};

	virtual void Draw(GraphicsContext& Context);
	virtual void BuildPSO(const RootSignature& rootSignature);

private:
	InstanceRenderData mInstanceData;
};

