#include "SimpleGeometryInstanceShader.h"
#include "L3DInputLayout.h"
SimpleGeometryInstanceShader::SimpleGeometryInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath)
:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath)
{


}

SimpleGeometryInstanceShader::~SimpleGeometryInstanceShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SimpleGeometryInstanceShader::GetInputLayout()
{
	return L3DInputLayout::InstancePosNormalTangentTexc;
}
