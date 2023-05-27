#pragma once
#include "Misc/RenderObject.h"
namespace SSAO
{
	void Initialize(void);

	void Shutdown(void);

	void Render(GraphicsContext& Context, MainConstants& RenderData);

	void DrawUI();

	extern bool Enable;
}
