#include "SimpleGeometryShadowMapShader.h"
SimpleGeometryShadowMapShader::SimpleGeometryShadowMapShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath, std::wstring DSShaderPath, std::wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

SimpleGeometryShadowMapShader::~SimpleGeometryShadowMapShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SimpleGeometryShadowMapShader::GetInputLayout()
{
	return InputLayout::InstancePosNormalTangentTexc;
}
