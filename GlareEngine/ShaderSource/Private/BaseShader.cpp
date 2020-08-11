#include "BaseShader.h"

BaseShader::BaseShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath):
mVSShaderPath(VSShaderPath),
mPSShaderPath(PSShaderPath),
mGSShaderPath(GSShaderPath)
{ 
    //vs shader
    if (mVSShaderPath != L"")
    {
        mVSShaders = L3DUtil::CompileShader(mVSShaderPath, nullptr, "VS", "vs_5_1");
    }
    if (mPSShaderPath != L"")
    {
        mPSShaders = L3DUtil::CompileShader(mPSShaderPath, nullptr, "PS", "ps_5_1");
    }
    if (mGSShaderPath != L"")
    {
        mGSShaders = L3DUtil::CompileShader(mGSShaderPath, nullptr, "GS", "gs_5_1");
    }
}

BaseShader::~BaseShader()
{
}




ComPtr<ID3DBlob> BaseShader::GetVSShader()
{
    return mVSShaders;
}

ComPtr<ID3DBlob> BaseShader::GetPSShader()
{
    return mPSShaders;
}

ComPtr<ID3DBlob> BaseShader::GetGSShader()
{
    return mGSShaders;
}


