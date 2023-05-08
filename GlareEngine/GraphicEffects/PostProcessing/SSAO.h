#pragma once
#include "Misc/RenderObject.h"
namespace SSAO
{
	void Initialize(void);
	void Shutdown(void);
	void Render(GraphicsContext& Context, const float* ProjMat, float NearClip, float FarClip);
	void Render(GraphicsContext& Context, Camera& camera);
	

	extern bool Enable;
	extern bool AsyncCompute;
	extern bool ComputeLinearZ;
}
