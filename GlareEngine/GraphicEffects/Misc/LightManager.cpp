#include "LightManager.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include "Graphics/CommandContext.h"
#include "Engine/Camera.h"
#include "Graphics/BufferManager.h"
#include "Math/VectorMath.h"


//shaders
#include "CompiledShaders/FillLightGrid_8_CS.h"
#include "CompiledShaders/FillLightGrid_16_CS.h"
#include "CompiledShaders/FillLightGrid_24_CS.h"
#include "CompiledShaders/FillLightGrid_32_CS.h"

using namespace GlareEngine::Math;
using namespace GlareEngine;

void Lighting::InitializeResources(void)
{
}

void Lighting::CreateRandomLights(const Vector3 minBound, const Vector3 maxBound)
{
}

void Lighting::FillLightGrid(GraphicsContext& gfxContext, const Camera& camera)
{
}

void Lighting::Shutdown(void)
{
}
