#include "ShadowMapShader.h"
#include "L3DInputLayout.h"
ShadowMapShader::ShadowMapShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath)
{
}

ShadowMapShader::~ShadowMapShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> ShadowMapShader::GetInputLayout()
{
	return L3DInputLayout::Pos;
}
