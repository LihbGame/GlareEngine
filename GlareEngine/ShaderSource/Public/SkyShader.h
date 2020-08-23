#pragma once
#include "BaseShader.h"
class SkyShader :
    public BaseShader
{
public:
    SkyShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", const D3D_SHADER_MACRO* defines = nullptr);
    ~SkyShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();



};

