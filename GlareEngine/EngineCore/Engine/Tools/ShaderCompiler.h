#pragma once

#include <d3d12shader.h>
#include <d3d12.h>
#include <string>
#include <map>
#include <memory>
#include <assert.h>

#define USE_RUNTIME_PSO 1

class ShaderDefinitions
{
public:
	void SetDefine(const char* name, const char* value) { m_Definitions[name] = value; }

	template<class T>
	void SetDefine(const char* name, T value) { m_Definitions[name] = (std::stringstream() << value).str(); }

	void Merge(const ShaderDefinitions& other)
	{
		for each (const auto & NameValue in other.m_Definitions)
		{
			SetDefine(NameValue.first.c_str(), NameValue.second);
		}
	}

	const std::map<std::string, std::string>& GetDefinitions() const { return m_Definitions; }
private:
	std::map<std::string, std::string> m_Definitions;
};

struct ShaderBinary
{
	ShaderBinary() = default;

	ShaderBinary(void* blob, size_t size)
	{
		if (blob && size)
		{
			Buffer.reset(new byte[size]());
			assert(memcpy_s(Buffer.get(), size, blob, size) == 0);
			ByteCode.BytecodeLength = size;
			ByteCode.pShaderBytecode = Buffer.get();
		}
	}

	bool IsValid() const { return ByteCode.BytecodeLength > 0u; }

	D3D12_SHADER_BYTECODE ByteCode{};
	std::shared_ptr<byte> Buffer;
};

class ShaderCompiler
{
public:
	static ShaderCompiler& Get();

	ShaderCompiler(bool useDxc = false);

	~ShaderCompiler();

	static const char* GetDefaultProfile(D3D12_SHADER_VERSION_TYPE shaderVersionType, bool useDxc)
	{
		switch (shaderVersionType)
		{
		case D3D12_SHVER_PIXEL_SHADER:
			return useDxc ? "ps_6_0" : "ps_5_1";
		case D3D12_SHVER_VERTEX_SHADER:
			return useDxc ? "vs_6_0" : "vs_5_1";
		case D3D12_SHVER_GEOMETRY_SHADER:
			return useDxc ? "gs_6_0" : "gs_5_1";
		case D3D12_SHVER_HULL_SHADER:
			return useDxc ? "hs_6_0" : "hs_5_1";
		case D3D12_SHVER_DOMAIN_SHADER:
			return useDxc ? "ds_6_0" : "ds_5_1";
		case D3D12_SHVER_COMPUTE_SHADER:
			return useDxc ? "cs_6_0" : "cs_5_1";
		default:
			return nullptr;
		}
	}

	ShaderBinary Compile(const char* shaderFilePath, const char* entryPoint, D3D12_SHADER_VERSION_TYPE shaderVersionType, const ShaderDefinitions& definitions)
	{
		return Compile(shaderFilePath, entryPoint, GetDefaultProfile(shaderVersionType, m_UseDxc), definitions);
	}

	ShaderBinary Compile(const char* shaderFilePath, const char* entryPoint, const char* profile, const ShaderDefinitions& definitions);
private:
	bool m_UseDxc;
	struct IDxcUtils* m_DxcUtils = nullptr;
	struct IDxcCompiler3* m_DxcCompiler = nullptr;
};
