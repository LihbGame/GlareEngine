#include "SimpleGeometryInstanceShader.h"
SimpleGeometryInstanceShader::SimpleGeometryInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath, defines)
{


}

SimpleGeometryInstanceShader::~SimpleGeometryInstanceShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SimpleGeometryInstanceShader::GetInputLayout()
{
	return L3DInputLayout::InstancePosNormalTangentTexc;
}
