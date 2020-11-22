#include "ShockWaveWaterShader.h"
ShockWaveWaterShader::ShockWaveWaterShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath, defines)
{
}

ShockWaveWaterShader::~ShockWaveWaterShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> ShockWaveWaterShader::GetInputLayout()
{
	return L3DInputLayout::PosNormalTangentTexc;
}

