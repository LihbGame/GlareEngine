#include "ComplexStaticModelInstanceShader.h"
ComplexStaticModelInstanceShader::ComplexStaticModelInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath, wstring DSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

ComplexStaticModelInstanceShader::~ComplexStaticModelInstanceShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> ComplexStaticModelInstanceShader::GetInputLayout()
{
	return InputLayout::PosNormalTangentTexc;
}