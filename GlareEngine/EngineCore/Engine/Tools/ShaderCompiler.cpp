#include "ShaderCompiler.h"
#include <d3dcompiler.h>
#include "Engine/FileUtility.h"
#include "Engine/EngineLog.h"
#include <../packages/dxc_2023_08_14/inc/dxcapi.h>

inline std::wstring string2wstring(const char* str)
{
	std::string strcopy(str);
	return std::wstring(strcopy.begin(), strcopy.end());
}

ShaderCompiler::ShaderCompiler(bool useDxc)
	: m_UseDxc(useDxc)
{
	assert(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)) == S_OK);
	assert(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler)) == S_OK);
}

ShaderCompiler& ShaderCompiler::Get()
{
	static std::unique_ptr<ShaderCompiler> s_ShaderCompiler;
	if (!s_ShaderCompiler)
	{
		s_ShaderCompiler = std::make_unique<ShaderCompiler>(true);
	}
	return *s_ShaderCompiler;
}

ShaderCompiler::~ShaderCompiler()
{
	m_DxcUtils->Release();
	m_DxcCompiler->Release();
}

ShaderBinary ShaderCompiler::Compile(const char* shaderFilePath, const char* entryPoint, const char* profile, const ShaderDefinitions& definitions)
{
	assert(shaderFilePath && entryPoint && profile);

	auto sourceCode = FileUtility::ReadFile(shaderFilePath, EFileMode::Text);
	auto sourceCodeSize = sourceCode->size();
	assert(sourceCodeSize);

	if (m_UseDxc)
	{
		IDxcBlobEncoding* blobEncoding = nullptr;
		assert(m_DxcUtils->CreateBlobFromPinned((void*)sourceCode->data(), static_cast<uint32_t>(sourceCodeSize), DXC_CP_ACP, &blobEncoding) == S_OK);

		using ShaderDefine = std::pair<std::wstring, std::wstring>;
		std::vector<ShaderDefine> shaderDefines(definitions.GetDefinitions().size());
		std::vector<DxcDefine> dxcDefines(definitions.GetDefinitions().size());

		uint32_t Index = 0u;
		for each (const auto& nameValue in definitions.GetDefinitions())
		{
			shaderDefines[Index].first = std::wstring(nameValue.first.begin(), nameValue.first.end());
			shaderDefines[Index].second = std::wstring(nameValue.second.begin(), nameValue.second.end());

			dxcDefines[Index].Name = shaderDefines[Index].first.c_str();
			dxcDefines[Index].Value = shaderDefines[Index].second.c_str();

			++Index;
		}

		std::vector<const wchar_t*> args
		{
#if defined(DEBUG) || defined(_DEBUG)
			L"-Zi"
#else
			L"Qstrip_debug",
			L"Qstrip_reflect"
#endif
		};

		IDxcCompilerArgs* compileArgs = nullptr;
		assert(m_DxcUtils->BuildArguments(
			string2wstring(shaderFilePath).c_str(),
			string2wstring(entryPoint).c_str(),
			string2wstring(profile).c_str(),
			args.data(),
			static_cast<uint32_t>(args.size()),
			dxcDefines.data(),
			static_cast<uint32_t>(dxcDefines.size()),
			&compileArgs) == S_OK);

		DxcBuffer dxcBuffer
		{
			blobEncoding->GetBufferPointer(),
			blobEncoding->GetBufferSize(),
			DXC_CP_ACP
		};

		IDxcIncludeHandler* includeHandler = nullptr;
		assert(m_DxcUtils->CreateDefaultIncludeHandler(&includeHandler) == S_OK);

		IDxcResult* result;
		assert(m_DxcCompiler->Compile(
			&dxcBuffer,
			compileArgs->GetArguments(),
			compileArgs->GetCount(),
			includeHandler,
			IID_PPV_ARGS(&result)) == S_OK);

		blobEncoding->Release();
		compileArgs->Release();

		::HRESULT hr = E_FAIL;
		assert(result->GetStatus(&hr) == S_OK);

		if (FAILED(hr))
		{
			IDxcBlobEncoding* error = nullptr;
			assert(result->GetErrorBuffer(&error) == S_OK);
			auto shaderFile = string2wstring(shaderFilePath);
			auto errorMsg = string2wstring(static_cast<const char*>(error->GetBufferPointer()));
			EngineLog::AddLog(L"ShaderCompiler: Faild to compile shader: \"%s\"\n%s", shaderFile.c_str(), errorMsg.c_str());
			error->Release();
			result->Release();

			return ShaderBinary();
		}
		else
		{
			IDxcBlob* blob = nullptr;
			assert(result->GetResult(&blob) == S_OK);

			ShaderBinary binary(blob->GetBufferPointer(), blob->GetBufferSize());

			result->Release();
			blob->Release();

			return binary;
		}
	}
	else
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error = nullptr;

		// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/d3dcompile-constants
		// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/d3dcompile-effect-constants
#if defined(DEBUG) || defined(_DEBUG)
		uint32_t Flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_DEBUG;
#else
		uint32_t Flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY; /// Forces strict compile, which might not allow for legacy syntax.
#endif

		uint32_t index = 0u;
		std::vector<D3D_SHADER_MACRO> d3dMacros(definitions.GetDefinitions().size());
		for each (const auto& NameValue in definitions.GetDefinitions())
		{
			d3dMacros[index].Name = NameValue.first.c_str();
			d3dMacros[index].Definition = NameValue.second.c_str();
			++index;
		}

		if (FAILED(::D3DCompile2(
			sourceCode->data(),
			sourceCodeSize,
			shaderFilePath,
			d3dMacros.data(),
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint,
			profile,
			Flags,
			0u,
			0u,
			nullptr,
			0u,
			&blob,
			&error)))
		{
			auto shaderFile = string2wstring(shaderFilePath);
			auto errorMsg = string2wstring(static_cast<const char*>(error->GetBufferPointer()));
			EngineLog::AddLog(L"ShaderCompiler: Faild to compile shader: \"%s\"\n%s", shaderFile.c_str(), errorMsg.c_str());
			error->Release();

			return ShaderBinary();
		}
		else
		{
			ShaderBinary binary(blob->GetBufferPointer(), blob->GetBufferSize());
			blob->Release();
			return binary;
		}
	}
}
