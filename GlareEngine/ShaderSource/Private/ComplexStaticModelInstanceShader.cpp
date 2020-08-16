#include "ComplexStaticModelInstanceShader.h"
#include "L3DInputLayout.h"
ComplexStaticModelInstanceShader::ComplexStaticModelInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath)
{
}

ComplexStaticModelInstanceShader::~ComplexStaticModelInstanceShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> ComplexStaticModelInstanceShader::GetInputLayout()
{
	return L3DInputLayout::PosNormalTangentTexc;
}