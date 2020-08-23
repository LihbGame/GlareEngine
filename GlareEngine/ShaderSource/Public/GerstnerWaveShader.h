#pragma once
#include "BaseShader.h"
class GerstnerWaveShader :
    public BaseShader
{
public:
    GerstnerWaveShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", const D3D_SHADER_MACRO* defines = nullptr);
    ~GerstnerWaveShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();

};

