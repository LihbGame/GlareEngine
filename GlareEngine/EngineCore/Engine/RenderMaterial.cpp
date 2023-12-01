#include "RenderMaterial.h"

RenderMaterial::RenderMaterial()
{
}

void RenderMaterial::BeginInitializeComputeMaterial(wstring MaterialName, const RootSignature& rootSignature)
{
	mComputePSO = ComputePSO(MaterialName);
	mComputePSO.SetRootSignature(rootSignature);
}

void RenderMaterial::BeginInitializeGraphicMaterial(wstring MaterialName, const RootSignature& rootSignature)
{
	mGraphicsPSO = GraphicsPSO(MaterialName);
}
