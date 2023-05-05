#include "SSAO.h"

namespace SSAO
{
	bool Enable = true;
	bool AsyncCompute = false;
	bool ComputeLinearZ = true;
}


void SSAO::Initialize(void)
{
}

void SSAO::Shutdown(void)
{
}

void SSAO::Render(GraphicsContext& Context, const float* ProjMat, float NearClip, float FarClip)
{

}

void SSAO::Render(GraphicsContext& Context, Camera& camera)
{
}

void SSAO::LinearizeZ(ComputeContext& Context, Camera& camera, uint32_t FrameIndex)
{
}

