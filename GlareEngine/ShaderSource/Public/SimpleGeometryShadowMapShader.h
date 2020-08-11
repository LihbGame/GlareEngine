#pragma once
#include "BaseShader.h"
class SimpleGeometryShadowMapShader :
    public BaseShader
{
public:
    SimpleGeometryShadowMapShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"");
    ~SimpleGeometryShadowMapShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

