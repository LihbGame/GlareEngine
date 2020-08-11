#pragma once
#include "BaseShader.h"
class ShadowMapShader :
    public BaseShader
{
    ShadowMapShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"");
    ~ShadowMapShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

