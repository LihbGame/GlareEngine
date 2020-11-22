#include "BaseShader.h"
BaseShader::BaseShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath, _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines):
mVSShaderPath(VSShaderPath),
mPSShaderPath(PSShaderPath),
mGSShaderPath(GSShaderPath)
{ 
    //vs shader
    if (mVSShaderPath != L"")
    {
        mVSShaders = L3DUtil::CompileShader(mVSShaderPath, defines, "VS", "vs_5_1");
    }
    if (mPSShaderPath != L"")
    {
        mPSShaders = L3DUtil::CompileShader(mPSShaderPath, defines, "PS", "ps_5_1");
    }
    if (mGSShaderPath != L"")
    {
        mGSShaders = L3DUtil::CompileShader(mGSShaderPath, defines, "GS", "gs_5_1");
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


