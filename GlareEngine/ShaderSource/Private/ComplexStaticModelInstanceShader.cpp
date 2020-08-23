#include "ComplexStaticModelInstanceShader.h"
#include "L3DInputLayout.h"
ComplexStaticModelInstanceShader::ComplexStaticModelInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath, defines)
{
}

ComplexStaticModelInstanceShader::~ComplexStaticModelInstanceShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> ComplexStaticModelInstanceShader::GetInputLayout()
{
	return L3DInputLayout::PosNormalTangentTexc;
}