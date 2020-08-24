#include "SkinAnimeShader.h"
#include "L3DInputLayout.h"
SkinAnimeShader::SkinAnimeShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, GSShaderPath, defines)
{
}

SkinAnimeShader::~SkinAnimeShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SkinAnimeShader::GetInputLayout()
{
	return L3DInputLayout::SkinAnimeVertex;
}