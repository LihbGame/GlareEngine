#include "HeightMapTerrainShader.h"

HeightMapTerrainShader::HeightMapTerrainShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath, wstring DSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

HeightMapTerrainShader::~HeightMapTerrainShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> HeightMapTerrainShader::GetInputLayout()
{
	return L3DInputLayout::HeightMapTerrain;
}
