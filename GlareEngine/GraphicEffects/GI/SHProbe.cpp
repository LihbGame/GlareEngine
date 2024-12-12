#include "SHProbe.h"
#include "Engine/Scene.h"
#include "Graphics/Render.h"
#include "Engine/EngineProfiling.h"

using namespace GlareEngine::Render;

SHProbe::~SHProbe()
{
}

SHProbe::SHProbe(XMFLOAT3 position, CubeRenderTarget& cubeTarget)
	:mProbePosition(position),
	mCubeTarget(cubeTarget)
{
	mCubeTarget.UpdateCubeMapPosition(mProbePosition);
}

void SHProbe::PreComputeSH()
{

}

void SHProbe::PrepareCubeMap(Scene& scene, GraphicsContext& Context)
{
	ScopedTimer PrepareCubeMapScope(L"SHProbe RenderScene", Context);
	mCubeTarget.CaptureScene(scene, Context);
}

void SHProbe::StoreSH()
{
}
