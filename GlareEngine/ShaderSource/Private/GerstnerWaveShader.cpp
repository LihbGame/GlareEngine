#include "GerstnerWaveShader.h"
GerstnerWaveShader::GerstnerWaveShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
:BaseShader(VSShaderPath,PSShaderPath,GSShaderPath, defines)
{
}

GerstnerWaveShader::~GerstnerWaveShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> GerstnerWaveShader::GetInputLayout()
{
	return L3DInputLayout::PosNormalTangentTexc;
}
