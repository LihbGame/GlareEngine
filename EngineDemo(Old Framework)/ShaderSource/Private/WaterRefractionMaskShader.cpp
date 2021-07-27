#include "WaterRefractionMaskShader.h"
WaterRefractionMaskShader::WaterRefractionMaskShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath, wstring DSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

WaterRefractionMaskShader::~WaterRefractionMaskShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> WaterRefractionMaskShader::GetInputLayout()
{
	return L3DInputLayout::Pos;
}
