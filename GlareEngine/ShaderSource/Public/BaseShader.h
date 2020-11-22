#pragma once
#include "L3DBaseApp.h"
#include "L3DMathHelper.h"
#include "L3DInputLayout.h"
class BaseShader
{
public:
	BaseShader(wstring VSShaderPath, wstring PSShaderPath, wstring GSShaderPath = L"", _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines=nullptr);
	~BaseShader();


	ComPtr<ID3DBlob> GetVSShader();
	ComPtr<ID3DBlob> GetPSShader();
	ComPtr<ID3DBlob> GetGSShader();
	virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout()=0;
private:
	ComPtr<ID3DBlob> mPSShaders;
	ComPtr<ID3DBlob> mVSShaders;
	ComPtr<ID3DBlob> mGSShaders;

	wstring mVSShaderPath;
	wstring mPSShaderPath;
	wstring mGSShaderPath;
protected:
};

