#include "RuntimePSOManager.h"
#include "../packages/filewatch/FileWatch.hpp"
#include "Engine/EngineThread.h"
#include <filesystem>

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
	//m_RuntimePSOThread.get()->detach();
}

void RuntimePSOManager::StartRuntimePSOThread()
{
	if (!m_StopRuntimePSOThread)
	{
		ShaderCompiler::Get();

		/*s_FileWatch = std::make_unique<filewatch::FileWatch<std::string>>(
			GetShaderAssetDirectory(),
			[this](const std::filesystem::path& path, const filewatch::Event change_type) {
				switch (change_type)
				{

				case filewatch::Event::modified:
					path.string();
					this->EnqueuePSOCreationTask(path.string());
					break;
				};
			}
		);*/

		EngineThread::Get().AddTask([&]() {RuntimePSOManager::RuntimePSOThreadFunc(); });

		//m_RuntimePSOThread = std::make_unique<std::thread>(&RuntimePSOManager::RuntimePSOThreadFunc, this);
	}
}

void RuntimePSOManager::RegisterPSO(PSO* origin, EPSOType type, const char* sourceShaderPath, D3D12_SHADER_VERSION_TYPE shaderType,ShaderDefinitions shaderDefine)
{
	assert(origin && sourceShaderPath);

	std::string shaderSource(sourceShaderPath);

	auto it = m_PSOProxies.find(shaderSource);
	if (it == m_PSOProxies.end())
	{
		m_PSOProxies.insert(std::make_pair(shaderSource, std::make_unique<PSOProxy>(origin, type)));
	}

	m_PSOProxies[shaderSource]->SetShaderSourceFilePath(sourceShaderPath, shaderType);
	m_PSOProxies[shaderSource]->ShaderBinaries[shaderType].ShaderDefine = shaderDefine;

	RegisterPSODependency(shaderSource);
}

void RuntimePSOManager::EnqueuePSOCreationTask()
{
	for (auto& pso : m_PSODependencies)
	{
		for (auto type = D3D12_SHVER_PIXEL_SHADER; type <= D3D12_SHVER_COMPUTE_SHADER; type = (D3D12_SHADER_VERSION_TYPE)(type + 1))
		{
			PSOProxy::Shader shader = pso.second->ShaderBinaries[type];
			
			if (shader.SourceFilePath)
			{
				Sleep(100);
				if (shader.LastWriteTime != FileUtility::GetFileLastWriteTime(shader.SourceFilePath))
				{
					std::lock_guard<std::mutex> locker(m_TaskMutex);
					m_Tasks.push_back(PSOCreationTask(pso.second));
					break;
				}
			}
		}
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
		EnqueuePSOCreationTask();

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
			PSOProxy::Shader& shader = Proxy->ShaderBinaries[type];
			if (shader.SourceFilePath)
			{
				auto lastWriteTime = shader.LastWriteTime;
				auto currentLastWriteTime = FileUtility::GetFileLastWriteTime(shader.SourceFilePath);
				if (currentLastWriteTime != lastWriteTime)
				{
					shader.LastWriteTime = currentLastWriteTime;
					Proxy->IsPSODirty.store(true);

					Sleep(100);
					auto binary = ShaderCompiler::Get().Compile(
						shader.SourceFilePath,
						"main",
						type,
						Proxy->ShaderBinaries[type].ShaderDefine);

					if (binary.IsValid())
					{
						shader.Binary = binary;

						if (Proxy->Type == EPSOType::Graphics)
						{
							if (!Proxy->RuntimePSO)
							{
								Proxy->RuntimePSO = std::make_shared<GraphicsPSO>();
							}

							auto gfxPSO = static_cast<GraphicsPSO*>(Proxy->RuntimePSO.get());
							auto originPSO = static_cast<GraphicsPSO*>(Proxy->OriginPSO);
							gfxPSO->SetPSODesc(originPSO->GetPSODesc());
							gfxPSO->SetRootSignature(originPSO->GetRootSignature());
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
							if (!Proxy->RuntimePSO)
							{
								Proxy->RuntimePSO = std::make_shared<ComputePSO>();
							}

							auto computePSO = static_cast<ComputePSO*>(Proxy->RuntimePSO.get());
							auto originPSO = static_cast<ComputePSO*>(Proxy->OriginPSO);
							computePSO->SetPSODesc(originPSO->GetPSODesc());
							computePSO->SetRootSignature(originPSO->GetRootSignature());
							assert(type == D3D12_SHVER_COMPUTE_SHADER);
							computePSO->SetComputeShader(binary.ByteCode);
							computePSO->Finalize();
							Proxy->IsRuntimePSOReady.store(true);
						}
					}
				}
			}
		}
	}
}
