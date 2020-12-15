#pragma once
#include "BaseShader.h"

class GrassShader :
	public BaseShader
{
public:
	GrassShader(wstring VSShaderPath, wstring PSShaderPath, wstring HSShaderPath = L"", wstring DSShaderPath = L"", wstring GSShaderPath = L"", _In_reads_opt_(_Inexpressible_(defines->Name != NULL)) const D3D_SHADER_MACRO* defines = nullptr);
	~GrassShader();
	virtual std::vector<D3D12_INPUT_ELEMENT_DESC> GetInputLayout();

};

