#include "GerstnerWaveShader.h"
#include "L3DInputLayout.h"
GerstnerWaveShader::GerstnerWaveShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath)
:BaseShader(VSShaderPath,PSShaderPath,GSShaderPath)
{
}

GerstnerWaveShader::~GerstnerWaveShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> GerstnerWaveShader::GetInputLayout()
{
	return L3DInputLayout::PosNormalTangentTexc;
}
