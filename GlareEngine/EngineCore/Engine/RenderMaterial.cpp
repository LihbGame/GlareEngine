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

void RenderMaterial::BuildMaterialPSO(const PSOCommonProperty CommonProperty)
{
	mFuntionPSO(CommonProperty);
}

RenderMaterial* RenderMaterialManager::GetMaterial(string MaterialName)
{
	if (mRenderMaterialMap.find(MaterialName) == mRenderMaterialMap.end())
	{
		mRenderMaterialMap[MaterialName] = RenderMaterial(StringToWString(MaterialName));
	}
	return &mRenderMaterialMap[MaterialName];
}

void RenderMaterialManager::BuildMaterialsPSO(const PSOCommonProperty CommonProperty)
{
	for (auto& Material:mRenderMaterialMap)
	{
		Material.second.BuildMaterialPSO(CommonProperty);
	}
}
