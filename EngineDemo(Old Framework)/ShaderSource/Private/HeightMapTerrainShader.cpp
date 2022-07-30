#include "HeightMapTerrainShader.h"

HeightMapTerrainShader::HeightMapTerrainShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath, std::wstring DSShaderPath, std::wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

HeightMapTerrainShader::~HeightMapTerrainShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> HeightMapTerrainShader::GetInputLayout()
{
	return InputLayout::HeightMapTerrain;
}
