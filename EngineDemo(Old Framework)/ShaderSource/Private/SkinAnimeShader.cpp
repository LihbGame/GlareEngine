#include "SkinAnimeShader.h"
SkinAnimeShader::SkinAnimeShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath, wstring DSShaderPath, wstring GSShaderPath, _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

SkinAnimeShader::~SkinAnimeShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SkinAnimeShader::GetInputLayout()
{
	return L3DInputLayout::SkinAnimeVertex;
}