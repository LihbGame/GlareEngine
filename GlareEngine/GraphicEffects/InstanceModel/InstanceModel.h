#pragma once

#include "Engine/EngineUtility.h"
#include "Engine/GeometryGenerator.h"
#include "Engine/Vertex.h"
#include "Misc/RenderObject.h"
#include "Engine/RenderMaterial.h"

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

	void Update(float dt, GraphicsContext* Context = nullptr) {}

private:
	RenderMaterial* mInstanceModelMaterial = nullptr;

	InstanceRenderData mInstanceData;
};

