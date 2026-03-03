#include "DLSS.h"
#include "Engine/EngineLog.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"
#include "EngineGUI.h"
#include "Graphics/DepthBuffer.h"

#include "DLSS/nvsdk_ngx_helpers.h"
#include "Engine/Camera.h"

using namespace std;

DLSS* DLSS::m_pDLSSInstance = nullptr;

DLSS* DLSS::GetInstance()
{
	if (m_pDLSSInstance == nullptr)
	{
		m_pDLSSInstance = new DLSS();
	}
	return m_pDLSSInstance;
}

void DLSS::Shutdown()
{
	if (m_pDLSSInstance)
	{
		delete m_pDLSSInstance;
		m_pDLSSInstance = nullptr;
	}
}

bool DLSS::InitializeDLSSFeatures(Math::UINT2 optimalRenderSize, Math::UINT2 displayOutSize, const DLSSSettings& settings)
{
	if (!IsNGXInitialized())
	{
		EngineLog::AddLog(L"NGX is not initialized. Cannot create DLSS feature.");
		return false;
	}

	m_Settings = settings;
	m_OptimalRenderSize = optimalRenderSize;
	m_DisplaySize = displayOutSize;

	CreateDLSSFeature(optimalRenderSize, displayOutSize);

	return m_dlssFeature != nullptr;
}

void DLSS::CreateDLSSFeature(Math::UINT2 optimalRenderSize, Math::UINT2 displayOutSize)
{
	NVSDK_NGX_Result Result = NVSDK_NGX_Result_Fail;

	m_ngxParameters->Set(NVSDK_NGX_Parameter_Width, optimalRenderSize.x);
	m_ngxParameters->Set(NVSDK_NGX_Parameter_Height, optimalRenderSize.y);
	m_ngxParameters->Set(NVSDK_NGX_Parameter_OutWidth, displayOutSize.x);
	m_ngxParameters->Set(NVSDK_NGX_Parameter_OutHeight, displayOutSize.y);
	m_ngxParameters->Set(NVSDK_NGX_Parameter_PerfQualityValue, m_Settings.Quality);

	if (m_Settings.EnableSharpening)
	{
		m_ngxParameters->Set(NVSDK_NGX_Parameter_Sharpness, m_Settings.Sharpness);
	}

	size_t ScratchSize = 0;
	Result = NVSDK_NGX_D3D12_GetScratchBufferSize(NVSDK_NGX_Feature_SuperSampling, m_ngxParameters, &ScratchSize);
	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"NVSDK_NGX_D3D12_GetScratchBufferSize failed, code = 0x%08x", Result);
		return;
	}

	ID3D12Resource* ScratchBuffer = nullptr;
	if (ScratchSize > 0)
	{
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ScratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		HRESULT hr = g_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&ScratchBuffer));

		if (SUCCEEDED(hr))
		{
			m_ngxParameters->Set(NVSDK_NGX_Parameter_Scratch, ScratchBuffer);
			m_ngxParameters->Set(NVSDK_NGX_Parameter_Scratch_SizeInBytes, (unsigned int)ScratchSize);
			ScratchBuffer->Release();
		}
	}

	Result = NVSDK_NGX_D3D12_CreateFeature(nullptr, NVSDK_NGX_Feature_SuperSampling, m_ngxParameters, &m_dlssFeature);

	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"Failed to create DLSS feature, code = 0x%08x", Result);
		m_dlssFeature = nullptr;
		return;
	}

	EngineLog::AddLog(L"DLSS feature created successfully! Render resolution: %ux%u, Output resolution: %ux%u", 
		optimalRenderSize.x, optimalRenderSize.y, displayOutSize.x, displayOutSize.y);
}

void DLSS::ReleaseDLSSFeatures()
{
	if (!IsNGXInitialized())
	{
		EngineLog::AddLog(L"Attempt to ReleaseDLSSFeatures without NGX being initialized.");
		return;
	}

	g_CommandManager.IdleGPU();

	NVSDK_NGX_Result ResultDLSS = (m_dlssFeature != nullptr) ? NVSDK_NGX_D3D12_ReleaseFeature(m_dlssFeature) : NVSDK_NGX_Result_Success;
	if (NVSDK_NGX_FAILED(ResultDLSS))
	{
		EngineLog::AddLog(L"Failed to release DLSS feature");
	}

	m_dlssFeature = nullptr;
}

bool DLSS::QueryOptimalSettings(Math::UINT2 inDisplaySize, NVSDK_NGX_PerfQuality_Value inQualValue, DLSSRecommendedSettings* outRecommendedSettings)
{
	if (!IsNGXInitialized())
	{
		outRecommendedSettings->m_ngxRecommendedOptimalRenderSize = inDisplaySize;
		outRecommendedSettings->m_ngxDynamicMaximumRenderSize = inDisplaySize;
		outRecommendedSettings->m_ngxDynamicMinimumRenderSize = inDisplaySize;

		EngineLog::AddLog(L"NGX was not initialized when querying Optimal Settings");
		return false;
	}

	outRecommendedSettings->m_ngxRecommendedOptimalRenderSize = inDisplaySize;
	outRecommendedSettings->m_ngxDynamicMaximumRenderSize = inDisplaySize;
	outRecommendedSettings->m_ngxDynamicMinimumRenderSize = inDisplaySize;
	outRecommendedSettings->m_ngxRecommendedSharpness = 0.0f;

	return false;
}

DLSS::DLSS()
{
	InitializeNGX(L".");
}

DLSS::~DLSS()
{
	if (IsNGXInitialized())
	{
		ReleaseDLSSFeatures();
	}
	ShutdownNGX();
}

void DLSS::InitializeNGX(const wchar_t* rwLogsFolder)
{
	NVSDK_NGX_Result Result = NVSDK_NGX_Result_Fail;

	Result = NVSDK_NGX_D3D12_Init(APP_ID, rwLogsFolder, g_Device);

	m_ngxInitialized = !NVSDK_NGX_FAILED(Result);
	if (!IsNGXInitialized())
	{
		if (Result == NVSDK_NGX_Result_FAIL_FeatureNotSupported || Result == NVSDK_NGX_Result_FAIL_PlatformError)
			EngineLog::AddLog(L"NVIDIA NGX not available on this hardware/platform.");
		else
			EngineLog::AddLog(L"Failed to initialize NGX");

		return;
	}

	Result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_ngxParameters);

	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"NVSDK_NGX_GetCapabilityParameters failed");
		ShutdownNGX();
		return;
	}

	int dlssAvailable = 0;
	NVSDK_NGX_Result ResultDLSS = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
	if (ResultDLSS != NVSDK_NGX_Result_Success || !dlssAvailable)
	{
		EngineLog::AddLog(L"NVIDIA DLSS not available on this hardware/platform.");
		ShutdownNGX();
		return;
	}

	m_bDlssAvailable = true;
	EngineLog::AddLog(L"NVIDIA DLSS is available!");
}

void DLSS::ShutdownNGX()
{
	if (IsNGXInitialized())
	{
		g_CommandManager.IdleGPU();

		if (m_dlssFeature != nullptr)
		{
			EngineLog::AddLog(L"Attempt to release NGX library before features have been released!");
			ReleaseDLSSFeatures();
		}

		NVSDK_NGX_D3D12_DestroyParameters(m_ngxParameters);
		NVSDK_NGX_D3D12_Shutdown1(nullptr);

		m_ngxInitialized = false;
	}
}

bool DLSS::FindAdapter(const std::wstring& targetName)
{
	if (m_Adapter != nullptr)
		return true;

	IDXGIFactory1* DXGIFactory;
	HRESULT hres = CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory));
	if (hres != S_OK)
	{
		EngineLog::AddLog(L"ERROR in CreateDXGIFactory.");
		return false;
	}

	unsigned int adapterNo = 0;
	while (SUCCEEDED(hres))
	{
		IDXGIAdapter* pAdapter;
		hres = DXGIFactory->EnumAdapters(adapterNo, &pAdapter);

		if (SUCCEEDED(hres))
		{
			DXGI_ADAPTER_DESC aDesc;
			pAdapter->GetDesc(&aDesc);

			if (targetName.length() == 0)
			{
				m_Adapter = pAdapter;
				return true;
			}

			std::wstring aName = aDesc.Description;

			if (aName.find(targetName) != std::string::npos)
			{
				m_Adapter = pAdapter;
				return true;
			}
		}

		adapterNo++;
	}

	return false;
}

void DLSS::Execute(ComputeContext& Context, ColorBuffer& InputColor, ColorBuffer& OutputColor, 
                   DepthBuffer* DepthBuffer, ColorBuffer* MotionVectorBuffer, 
                   const Camera& camera, float deltaTime, bool reset)
{
	if (!IsDLSSInitialized() || !m_Settings.Enable)
	{
		return;
	}

	ID3D12GraphicsCommandList* CommandList = Context.GetCommandList();

	NVSDK_NGX_D3D12_DLSS_Eval_Params EvalParams = {};
	EvalParams.Feature.pInColor = InputColor.GetResource();
	EvalParams.Feature.pInOutput = OutputColor.GetResource();
	EvalParams.InRenderSubrectDimensions = { InputColor.GetWidth(), InputColor.GetHeight() };
	EvalParams.InJitterOffsetX = m_JitterOffset.x;
	EvalParams.InJitterOffsetY = m_JitterOffset.y;
	EvalParams.InReset = reset ? 1 : 0;
	EvalParams.InMVScaleX = 1.0f;
	EvalParams.InMVScaleY = 1.0f;
	EvalParams.Feature.InSharpness = m_Settings.EnableSharpening ? m_Settings.Sharpness : 0.0f;

	if (DepthBuffer != nullptr && DepthBuffer->GetResource())
	{
		EvalParams.pInDepth = DepthBuffer->GetResource();
	}

	if (MotionVectorBuffer != nullptr && MotionVectorBuffer->GetResource())
	{
		EvalParams.pInMotionVectors = MotionVectorBuffer->GetResource();
	}

	NVSDK_NGX_Result Result = NGX_D3D12_EVALUATE_DLSS_EXT(CommandList, m_dlssFeature, m_ngxParameters, &EvalParams);

	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"DLSS EvaluateFeature failed, code = 0x%08x", Result);
	}
}

void DLSS::UpdateJitter(int frameIndex)
{
	if (!IsDLSSInitialized() || !m_Settings.Enable)
	{
		m_JitterOffset = XMFLOAT2(0, 0);
		return;
	}

	static const int c_DLSSJitterPattern[] = {
		-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		-8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7
	};
	static const int c_DLSSJitterPatternCount = sizeof(c_DLSSJitterPattern) / sizeof(c_DLSSJitterPattern[0]);

	m_JitterIndex = frameIndex % c_DLSSJitterPatternCount;

	float jitterX = (float)c_DLSSJitterPattern[m_JitterIndex] / (float)m_OptimalRenderSize.x;
	float jitterY = (float)c_DLSSJitterPattern[m_JitterIndex] / (float)m_OptimalRenderSize.y;

	m_JitterOffset = XMFLOAT2(jitterX, jitterY);
}

void DLSS::DrawUI()
{
	ImGui::Separator();
	ImGui::Text("NVIDIA DLSS");

	if (!IsNGXInitialized())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DLSS not available (No NVIDIA GPU or outdated driver)");
		return;
	}

	if (!IsDLSSAvailable())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DLSS not supported on this hardware");
		return;
	}

	ImGui::Checkbox("Enable DLSS", &m_Settings.Enable);

	if (m_Settings.Enable && IsDLSSInitialized())
	{
		ImGui::Text("Render Resolution: %ux%u", m_OptimalRenderSize.x, m_OptimalRenderSize.y);
		ImGui::Text("Output Resolution: %ux%u", m_DisplaySize.x, m_DisplaySize.y);

		const char* qualityNames[] = { "Ultra Performance", "Performance", "Balanced", "Quality", "Ultra Quality" };
		int currentQuality = 0;
		switch (m_Settings.Quality)
		{
		case NVSDK_NGX_PerfQuality_Value_UltraPerformance: currentQuality = 0; break;
		case NVSDK_NGX_PerfQuality_Value_MaxPerf: currentQuality = 1; break;
		case NVSDK_NGX_PerfQuality_Value_Balanced: currentQuality = 2; break;
		case NVSDK_NGX_PerfQuality_Value_MaxQuality: currentQuality = 3; break;
		case NVSDK_NGX_PerfQuality_Value_DLAA: currentQuality = 4; break;
		}

		if (ImGui::Combo("Quality", &currentQuality, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			switch (currentQuality)
			{
			case 0: m_Settings.Quality = NVSDK_NGX_PerfQuality_Value_UltraPerformance; break;
			case 1: m_Settings.Quality = NVSDK_NGX_PerfQuality_Value_MaxPerf; break;
			case 2: m_Settings.Quality = NVSDK_NGX_PerfQuality_Value_Balanced; break;
			case 3: m_Settings.Quality = NVSDK_NGX_PerfQuality_Value_MaxQuality; break;
			case 4: m_Settings.Quality = NVSDK_NGX_PerfQuality_Value_DLAA; break;
			}
		}

		ImGui::Checkbox("Enable Sharpening", &m_Settings.EnableSharpening);

		if (m_Settings.EnableSharpening)
		{
			ImGui::SliderFloat("Sharpness", &m_Settings.Sharpness, 0.0f, 1.0f);
		}

		if (ImGui::Button("Reset DLSS"))
		{
			ReleaseDLSSFeatures();
			CreateDLSSFeature(m_OptimalRenderSize, m_DisplaySize);
		}
	}
}
