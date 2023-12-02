#pragma once

#include "Graphics/PipelineState.h"
#include "ShaderCompiler.h"
#include "Engine/FileUtility.h"
#include <array>
#include <atomic>
#include <deque>

#ifdef DEBUG
//use for runtime debug
#define USE_RUNTIME_PSO 1
#endif


#if USE_RUNTIME_PSO
#define GET_PSO(TargetPSO) TargetPSO.GetProxy()->Get()
#else
#define GET_PSO(TargetPSO) TargetPSO
#endif

#define SHADER_ASSET_DIRECTOTY   "../../GlareEngine/Shaders/"

#define GET_SHADER_PATH(ShaderPath) (std::string(SHADER_ASSET_DIRECTOTY) + ShaderPath).c_str()

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
		string SourceFilePath = "";
		std::time_t LastWriteTime = 0;
		ShaderDefinitions ShaderDefine;
		bool IsDirty = true;
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
		ShaderBinaries[type].SourceFilePath = std::string((char*)shaderFilePath);
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
		if (!RuntimePSO.get())
		{
			return *OriginPSO;
		}
		else
		{
			return *(IsPSODirty.load() ? (IsRuntimePSOReady.load() ? RuntimePSO.get() : OriginPSO) : OriginPSO);
		}
	}

	PSO* OriginPSO = nullptr;
	std::shared_ptr<PSO> RuntimePSO = nullptr;

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

	void DeletePSO(PSO* deletePSO) { 
		m_PSOProxies.erase(deletePSO); 
		m_PSODependencies.erase(deletePSO); 
	}
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

	void RegisterPSODependency(const std::string& shaderFile, const PSO* origin);

	void RuntimePSOThreadFunc();

	std::map<const PSO*, std::unique_ptr<PSOProxy>> m_PSOProxies;
	std::map<const PSO*, PSOProxy*> m_PSODependencies;

	//std::unique_ptr<std::thread> m_RuntimePSOThread;
	std::deque<PSOCreationTask> m_Tasks;

	std::mutex m_TaskMutex;

	bool m_StopRuntimePSOThread = false;
};
