#pragma once
#include "BaseShader.h"
class GerstnerWaveShader :
    public BaseShader
{
public:
    GerstnerWaveShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"");
    ~GerstnerWaveShader();
    virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();

};

