#include "RenderMaterial.h"

GlareEngine::RenderMaterial::RenderMaterial(wstring MaterialName)
	:mMaterialName(MaterialName)
{
}

void RenderMaterial::BeginInitializeComputeMaterial(const RootSignature& rootSignature)
{
	m_pRootSignature = &rootSignature;
	mComputePSO = ComputePSO(mMaterialName);
	mComputePSO.SetRootSignature(rootSignature);
}

void RenderMaterial::BeginInitializeGraphicMaterial(const RootSignature& rootSignature)
{
	mGraphicsPSO = GraphicsPSO(mMaterialName);
	m_pRootSignature = &rootSignature;
}

