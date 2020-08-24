#pragma once
#include "BaseShader.h"
class SkinAnimeShader :
    public BaseShader
{
public:
    SkinAnimeShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", const D3D_SHADER_MACRO* defines = nullptr);
    ~SkinAnimeShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

