#pragma once
#include "Misc/RenderObject.h"
namespace SSAO
{
	void Initialize(void);

	void Shutdown(void);

	void Render(GraphicsContext& Context, const Matrix4& ProjMat);

	void LinearizeZ(ComputeContext& Context, Camera& camera, uint32_t FrameIndex);

	void LinearizeZ(ComputeContext& Context, DepthBuffer& Depth, ColorBuffer& LinearDepth, float zMagic);

	extern bool Enable;

	extern bool AsyncCompute;

	extern bool ComputeLinearZ;
}
