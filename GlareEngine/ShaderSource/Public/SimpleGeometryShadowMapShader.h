#pragma once
#include "BaseShader.h"
class SimpleGeometryShadowMapShader :
    public BaseShader
{
public:
    SimpleGeometryShadowMapShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", const D3D_SHADER_MACRO* defines = nullptr);
    ~SimpleGeometryShadowMapShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

