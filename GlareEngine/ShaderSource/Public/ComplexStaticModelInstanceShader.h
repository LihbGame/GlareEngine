#pragma once
#include "BaseShader.h"
class ComplexStaticModelInstanceShader :
    public BaseShader
{
public:
    ComplexStaticModelInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", const D3D_SHADER_MACRO* defines = nullptr);
    ~ComplexStaticModelInstanceShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

