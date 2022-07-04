#pragma once
#include "RenderObject.h"
class glTFInstanceModel :
    public RenderObject
{
public:
	glTFInstanceModel() {};
	~glTFInstanceModel() {};

	static void Initialize();

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) {};

	virtual void DrawUI() {}

	static void BuildPSO(const PSOCommonProperty CommonProperty) {};

	void Update(float dt) {}
public:




};

