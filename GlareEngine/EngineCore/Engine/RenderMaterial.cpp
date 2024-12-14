#include "RenderMaterial.h"

GlareEngine::RenderMaterial::RenderMaterial(wstring MaterialName,MaterialPipelineType PipelineType)
	:mMaterialName(MaterialName),
	mPipelineType(PipelineType)
{
}

void RenderMaterial::BeginInitializeComputeMaterial(const RootSignature& rootSignature)
{
	m_pRootSignature = &rootSignature;
	mComputePSO = ComputePSO(mMaterialName);
	mComputePSO.SetRootSignature(rootSignature);
}

void RenderMaterial::BuildMaterialPSO(const PSOCommonProperty CommonProperty)
{
	mBuildPSOFunction(CommonProperty);
}

void RenderMaterial::InitRuntimePSO()
{
	mRuntimeModifyPSOFunction();
}

PSO& RenderMaterial::GetRuntimePSO()
{
	if (mPipelineType == MaterialPipelineType::Graphics)
	{
		return GET_PSO(mGraphicsPSO);
	}
	else
	{
		return GET_PSO(mComputePSO);
	}
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

void RenderMaterialManager::InitRuntimePSO()
{
	for (auto& Material : mRenderMaterialMap)
	{
		Material.second.InitRuntimePSO();
	}
}
