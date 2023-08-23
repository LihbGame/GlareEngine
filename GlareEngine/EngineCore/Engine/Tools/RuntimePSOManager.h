#pragma once

#include "Graphics/PipelineState.h"
#include "ShaderCompiler.h"
#include "Engine/FileUtility.h"
#include <array>
#include <atomic>
#include <deque>

#if USE_RUNTIME_PSO
#define GET_PSO(TargetPSO) TargetPSO.GetProxy()->Get()
#else
#define GET_PSO(TargetPSO) TargetPSO
#endif

#define SHADER_ASSET_DIRECTOTY   "../../GlareEngine/Shaders/"


enum class EPSOType
{
	Graphics,
	Compute
};

struct PSOProxy
{
	struct Shader
	{
		ShaderBinary Binary;
		const char* SourceFilePath = nullptr;
		std::time_t LastWriteTime = 0;
		ShaderDefinitions ShaderDefine;
	};

	PSOProxy(PSO* origin, EPSOType type)
		: OriginPSO(origin)
		, Type(type)
	{
		assert(origin);
		origin->SetProxy(this);
	}

	void SetShaderSourceFilePath(const char* shaderFilePath, D3D12_SHADER_VERSION_TYPE type)
	{
		ShaderBinaries[type].SourceFilePath = shaderFilePath;
		ShaderBinaries[type].LastWriteTime = FileUtility::GetFileLastWriteTime(shaderFilePath);
	}

	void SetShaderBinary(const ShaderBinary& binary, D3D12_SHADER_VERSION_TYPE type)
	{
		ShaderBinaries[type].Binary = binary;
	}

	GraphicsPSO& GetGraphics() const
	{
		assert(Type == EPSOType::Graphics);
		return *static_cast<GraphicsPSO*>(IsPSODirty.load() ? (IsRuntimePSOReady.load() ? RuntimePSO.get() : OriginPSO) : OriginPSO);
	}

	ComputePSO& GetCompute() const
	{
		assert(Type == EPSOType::Compute);
		return *static_cast<ComputePSO*>(IsPSODirty.load() ? (IsRuntimePSOReady.load() ? RuntimePSO.get() : OriginPSO) : OriginPSO);
	}

	PSO& Get() const
	{
		return *(IsPSODirty.load() ? (IsRuntimePSOReady.load() ? RuntimePSO.get() : OriginPSO) : OriginPSO);
	}

	PSO* OriginPSO = nullptr;
	std::shared_ptr<PSO> RuntimePSO;

	EPSOType Type = EPSOType::Graphics;
	std::atomic<bool> IsRuntimePSOReady = false;
	std::atomic<bool> IsPSODirty = false;
	std::array<Shader, D3D12_SHADER_VERSION_TYPE::D3D12_SHVER_COMPUTE_SHADER + 1u> ShaderBinaries;
};

class RuntimePSOManager
{
public:
	static RuntimePSOManager& Get();

	~RuntimePSOManager();

	void StartRuntimePSOThread();

	void RegisterPSO(GraphicsPSO* origin, const char* sourceShaderPath, D3D12_SHADER_VERSION_TYPE shaderType, ShaderDefinitions shaderDefine = ShaderDefinitions())
	{
		RegisterPSO(origin, EPSOType::Graphics, sourceShaderPath, shaderType,shaderDefine);
	}

	void RegisterPSO(ComputePSO* origin, const char* sourceShaderPath, D3D12_SHADER_VERSION_TYPE shaderType, ShaderDefinitions shaderDefine = ShaderDefinitions())
	{
		RegisterPSO(origin, EPSOType::Compute, sourceShaderPath, shaderType, shaderDefine);
	}

	void RegisterPSO(PSO* origin, EPSOType type, const char* sourceShaderPath, D3D12_SHADER_VERSION_TYPE shaderType, ShaderDefinitions shaderDefine);

	void EnqueuePSOCreationTask();

	void SetStopRuntimePSOThread(bool IsStop) { m_StopRuntimePSOThread = IsStop; }
private:
	struct PSOCreationTask
	{
		PSOCreationTask() = default;

		PSOCreationTask(PSOProxy* psoProxy)
			: Proxy(psoProxy)
		{
		}

		void Execute();

		PSOProxy* Proxy;
	};

	static std::vector<std::string> ParseShaderIncludeFiles(const char* sourceShaderPath);

	static std::string GetShaderAssetDirectory();

	void RegisterPSODependency(const std::string& shaderFile);

	void RuntimePSOThreadFunc();

	std::unordered_map<std::string, std::unique_ptr<PSOProxy>> m_PSOProxies;
	std::unordered_map<std::string, PSOProxy*> m_PSODependencies;
	//std::unique_ptr<std::thread> m_RuntimePSOThread;
	std::deque<PSOCreationTask> m_Tasks;

	std::mutex m_TaskMutex;

	bool m_StopRuntimePSOThread = false;
};
