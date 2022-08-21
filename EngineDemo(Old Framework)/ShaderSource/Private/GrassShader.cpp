#include "GrassShader.h"
GrassShader::GrassShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath, std::wstring DSShaderPath, std::wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

GrassShader::~GrassShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> GrassShader::GetInputLayout()
{
	return InputLayout::Grass;
}