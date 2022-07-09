#pragma once
#include "RenderObject.h"
class glTFInstanceModel :
    public RenderObject
{
public:
	glTFInstanceModel() {};
	~glTFInstanceModel() {};

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) {};

	virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr) {};

	virtual void DrawUI() {}

	static void BuildPSO(const PSOCommonProperty CommonProperty);

	static uint8_t GetPSO(uint16_t psoFlags);

	void Update(float dt) {}
public:




};

