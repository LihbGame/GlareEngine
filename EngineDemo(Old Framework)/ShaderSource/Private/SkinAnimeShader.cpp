#include "SkinAnimeShader.h"
SkinAnimeShader::SkinAnimeShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath, std::wstring DSShaderPath, std::wstring GSShaderPath, _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines)
	:BaseShader(VSShaderPath, PSShaderPath, HSShaderPath, DSShaderPath, GSShaderPath, defines)
{
}

SkinAnimeShader::~SkinAnimeShader()
{
}

std::vector<D3D12_INPUT_ELEMENT_DESC> SkinAnimeShader::GetInputLayout()
{
	return InputLayout::SkinAnimeVertex;
}