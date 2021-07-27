#include "SimpleGeometryInstanceShader.h"
SimpleGeometryInstanceShader::SimpleGeometryInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath, wstring DSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{


}

SimpleGeometryInstanceShader::~SimpleGeometryInstanceShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SimpleGeometryInstanceShader::GetInputLayout()
{
	return L3DInputLayout::InstancePosNormalTangentTexc;
}
