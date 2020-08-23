#include "SkyShader.h"
#include "L3DInputLayout.h"
SkyShader::SkyShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath, defines)
{
}

SkyShader::~SkyShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SkyShader::GetInputLayout()
{
	return L3DInputLayout::Pos;
}
