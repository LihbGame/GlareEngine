#pragma once
#include "BaseShader.h"
class SimpleGeometryInstanceShader :
    public BaseShader
{
public:
    SimpleGeometryInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", const D3D_SHADER_MACRO* defines = nullptr);
    ~SimpleGeometryInstanceShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

