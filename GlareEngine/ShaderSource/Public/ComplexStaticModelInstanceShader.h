#pragma once
#include "BaseShader.h"
class ComplexStaticModelInstanceShader :
    public BaseShader
{
public:
    ComplexStaticModelInstanceShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"");
    ~ComplexStaticModelInstanceShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();
};

