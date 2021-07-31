#include "SkyShader.h"
SkyShader::SkyShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath, wstring DSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

SkyShader::~SkyShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SkyShader::GetInputLayout()
{
	return InputLayout::Pos;
}
