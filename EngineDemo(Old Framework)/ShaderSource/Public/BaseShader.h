#pragma once

#include "MathHelper.h"
#include "InputLayout.h"

class BaseShader
{
public:
	BaseShader(std::wstring VSShaderPath, std::wstring PSShaderPath, std::wstring HSShaderPath = L"", std::wstring DSShaderPath = L"", std::wstring GSShaderPath = L"", _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines = nullptr);
	~BaseShader();


	ComPtr<ID3DBlob> GetVSShader();
	ComPtr<ID3DBlob> GetPSShader();
	ComPtr<ID3DBlob> GetGSShader();
	ComPtr<ID3DBlob> GetHSShader();
	ComPtr<ID3DBlob> GetDSShader();

	virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout() = 0;
private:
	ComPtr<ID3DBlob> mPSShaders;
	ComPtr<ID3DBlob> mVSShaders;
	ComPtr<ID3DBlob> mGSShaders;
	ComPtr<ID3DBlob> mHSShaders;
	ComPtr<ID3DBlob> mDSShaders;

	std::wstring mVSShaderPath;
	std::wstring mPSShaderPath;
	std::wstring mGSShaderPath;
	std::wstring mHSShaderPath;
	std::wstring mDSShaderPath;
protected:
};

