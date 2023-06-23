#pragma once
#include "Misc/RenderObject.h"

namespace FXAA
{
	extern bool IsEnable;

	void Initialize(void);
	void Render(ComputeContext& Context, ColorBuffer* Input, ColorBuffer* Output);
	void DrawUI();
};

