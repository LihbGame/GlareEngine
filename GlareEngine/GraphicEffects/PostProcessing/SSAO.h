#pragma once
#include "Misc/RenderObject.h"
namespace SSAO
{
	void Initialize(void);
	void Shutdown(void);
	void Render(GraphicsContext& Context, const float* ProjMat, float NearClip, float FarClip);
	void Render(GraphicsContext& Context, Camera& camera);
	void LinearizeZ(ComputeContext& Context, Camera& camera, uint32_t FrameIndex);

	extern bool Enable;
	extern bool AsyncCompute;
	extern bool ComputeLinearZ;
}
