#include "RuntimePSOManager.h"
#include "../packages/filewatch/FileWatch.hpp"

static std::unique_ptr<filewatch::FileWatch<std::string>> s_FileWatch;

RuntimePSOManager& RuntimePSOManager::Get()
{
	static RuntimePSOManager s_RuntimePSOManager;
	return s_RuntimePSOManager;
}

RuntimePSOManager::~RuntimePSOManager()
{
	m_StopRuntimePSOThread = true;
	s_FileWatch.reset();
}

void RuntimePSOManager::StartRuntimePSOThread()
{
	if (!m_StopRuntimePSOThread)
	{
		ShaderCompiler::Get();

		s_FileWatch = std::make_unique<filewatch::FileWatch<std::string>>(
			GetShaderAssetDirectory(),
			[this](const std::string& path, const filewatch::Event change_type) {
				switch (change_type)
				{
				case filewatch::Event::modified:
					this->EnqueuePSOCreationTask(path);
					break;
				};
			}
		);

		m_RuntimePSOThread = std::make_unique<std::thread>(&RuntimePSOManager::RuntimePSOThreadFunc, this);
	}
}

void RuntimePSOManager::RegisterPSO(PSO* origin, EPSOType type, const char* sourceShaderPath, D3D12_SHADER_VERSION_TYPE shaderType)
{
	assert(origin && sourceShaderPath);

	std::string shaderSource(sourceShaderPath);

	auto it = m_PSOProxies.find(shaderSource);
	if (it == m_PSOProxies.end())
	{
		m_PSOProxies.insert(std::make_pair(shaderSource, std::make_unique<PSOProxy>(origin, type)));
	}

	it->second->SetShaderSourceFilePath(sourceShaderPath, shaderType);

	RegisterPSODependency(shaderSource);
}

void RuntimePSOManager::EnqueuePSOCreationTask(const std::string& shaderFile)
{
	auto it = m_PSODependencies.find(shaderFile);
	if (it != m_PSODependencies.end())
	{
		RegisterPSODependency(shaderFile);

		std::lock_guard<std::mutex> locker(m_TaskMutex);
		m_Tasks.push_back(PSOCreationTask(it->second));
	}
}

std::vector<std::string> RuntimePSOManager::ParseShaderIncludeFiles(const char* sourceShaderPath)
{
	//Todo :not support include files changes for now
	return std::vector<std::string>();
}

std::string RuntimePSOManager::GetShaderAssetDirectory()
{
	return SHADER_ASSET_DIRECTOTY;
}

void RuntimePSOManager::RegisterPSODependency(const std::string& shaderFile)
{
	auto it = m_PSOProxies.find(shaderFile);
	if (it != m_PSOProxies.end())
	{
		m_PSODependencies[shaderFile] = it->second.get();
		for (auto& include : ParseShaderIncludeFiles(shaderFile.c_str()))
		{
			m_PSODependencies[include] = it->second.get();
		}
	}
}

void RuntimePSOManager::RuntimePSOThreadFunc()
{
	PSOCreationTask task;
	while (true)
	{
		if (m_StopRuntimePSOThread && m_Tasks.empty())
		{
			break;
		}

		{
			std::lock_guard<std::mutex> locker(m_TaskMutex);

			if (!m_Tasks.empty())
			{
				task = m_Tasks.front();
				m_Tasks.pop_front();
				task.Execute();
			}
		}
	}
}

void RuntimePSOManager::PSOCreationTask::Execute()
{
	if (Proxy)
	{
		ShaderDefinitions definitions;

		for (auto type = D3D12_SHVER_PIXEL_SHADER; type <= D3D12_SHVER_COMPUTE_SHADER; type = (D3D12_SHADER_VERSION_TYPE)(type + 1))
		{
			auto shader = Proxy->ShaderBinaries[type];
			if (shader.SourceFilePath)
			{
				auto lastWriteTime = shader.LastWriteTime;
				auto currentLastWriteTime = FileUtility::GetFileLastWriteTime(shader.SourceFilePath);
				if (currentLastWriteTime != lastWriteTime)
				{
					shader.LastWriteTime = currentLastWriteTime;
					Proxy->IsPSODirty.store(true);
					if (!Proxy->RuntimePSO)
					{
						Proxy->RuntimePSO = std::make_shared<PSO>();
					}
					*Proxy->RuntimePSO = *Proxy->OriginPSO;

					auto binary = ShaderCompiler::Get().Compile(
						shader.SourceFilePath,
						"",
						type,
						definitions);

					if (binary.IsValid())
					{
						shader.Binary = binary;

						if (Proxy->Type == EPSOType::Graphics)
						{
							auto gfxPSO = static_cast<GraphicsPSO*>(Proxy->RuntimePSO.get());
							switch (type)
							{
							case D3D12_SHVER_PIXEL_SHADER:
								gfxPSO->SetPixelShader(binary.ByteCode);
								break;
							case D3D12_SHVER_VERTEX_SHADER:
								gfxPSO->SetVertexShader(binary.ByteCode);
								break;
							case D3D12_SHVER_GEOMETRY_SHADER:
								gfxPSO->SetGeometryShader(binary.ByteCode);
								break;
							case D3D12_SHVER_HULL_SHADER:
								gfxPSO->SetHullShader(binary.ByteCode);
								break;
							case D3D12_SHVER_DOMAIN_SHADER:
								gfxPSO->SetDomainShader(binary.ByteCode);
								break;
							}
							gfxPSO->Finalize();
						}
						else
						{
							auto computePSO = static_cast<ComputePSO*>(Proxy->RuntimePSO.get());
							assert(type == D3D12_SHVER_COMPUTE_SHADER);
							computePSO->SetComputeShader(binary.ByteCode);
							computePSO->Finalize();
						}
					}
				}
			}
		}
	}
}
