#include "SimpleGeometryShadowMapShader.h"
#include "L3DInputLayout.h"
SimpleGeometryShadowMapShader::SimpleGeometryShadowMapShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath, defines)
{
}

SimpleGeometryShadowMapShader::~SimpleGeometryShadowMapShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SimpleGeometryShadowMapShader::GetInputLayout()
{
	return L3DInputLayout::InstancePosNormalTangentTexc;
}
