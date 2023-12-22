#include "RenderMaterial.h"

GlareEngine::RenderMaterial::RenderMaterial(wstring MaterialName)
	:mMaterialName(MaterialName)
{
}

void RenderMaterial::BeginInitializeComputeMaterial(const RootSignature& rootSignature)
{
	mComputePSO = ComputePSO(mMaterialName);
	mComputePSO.SetRootSignature(rootSignature);
}

void RenderMaterial::BeginInitializeGraphicMaterial(const RootSignature& rootSignature)
{
	mGraphicsPSO = GraphicsPSO(mMaterialName);
}

