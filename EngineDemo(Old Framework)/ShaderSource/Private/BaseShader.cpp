#include "BaseShader.h"
BaseShader::BaseShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath , wstring DSShaderPath , wstring GSShaderPath, _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines):
mVSShaderPath(VSShaderPath),
mPSShaderPath(PSShaderPath),
mGSShaderPath(GSShaderPath),
mHSShaderPath(HSShaderPath),
mDSShaderPath(DSShaderPath)
{ 
    //vs shader
    if (mVSShaderPath != L"")
    {
        mVSShaders = EngineUtility::CompileShader(mVSShaderPath, defines, "VS", "vs_5_1");
    }
    if (mPSShaderPath != L"")
    {
        mPSShaders = EngineUtility::CompileShader(mPSShaderPath, defines, "PS", "ps_5_1");
    }
    if (mGSShaderPath != L"")
    {
        mGSShaders = EngineUtility::CompileShader(mGSShaderPath, defines, "GS", "gs_5_1");
    }
	if (mHSShaderPath != L"")
	{
		mHSShaders = EngineUtility::CompileShader(mHSShaderPath, defines, "HS", "hs_5_1");
	}
	if (mDSShaderPath != L"")
	{
		mDSShaders = EngineUtility::CompileShader(mDSShaderPath, defines, "DS", "ds_5_1");
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

ComPtr<ID3DBlob> BaseShader::GetHSShader()
{
    return mHSShaders;
}

ComPtr<ID3DBlob> BaseShader::GetDSShader()
{
    return mDSShaders;
}


