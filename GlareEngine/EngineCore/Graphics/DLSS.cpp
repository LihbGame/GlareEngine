#include "DLSS.h"
#include "Engine/EngineLog.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"
#include "EngineGUI.h"
#include "Graphics/DepthBuffer.h"
#include "Graphics/GraphicsCore.h"

#include "DLSS/nvsdk_ngx_helpers.h"
#include "Engine/Camera.h"
#include "Display.h"

using namespace std;

DLSS* DLSS::m_pDLSSInstance = nullptr;

constexpr size_t NUM_OFFSET_SEQUENCES = 64; // Use a large number of Halton sequence offsets to accomodate large scaling ratios.

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

	// Calculate upscale ratio and mip bias
	UpdateUpscaleRatio();

	CreateDLSSFeature(optimalRenderSize, displayOutSize);

	return m_dlssFeature != nullptr;
}

void DLSS::CreateDLSSFeature(Math::UINT2 optimalRenderSize, Math::UINT2 displayOutSize)
{
	NVSDK_NGX_Result Result = NVSDK_NGX_Result_Fail;

	unsigned int CreationNodeMask = 1;
	unsigned int VisibilityNodeMask = 1;

	// DLSS feature flags
	int DlssCreateFeatureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
	DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;  // Motion vectors at low resolution
	//DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
	DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;  // Enable if using HDR
	DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;  // Engine uses Reverse Z
	if (m_Settings.EnableSharpening)
	{
		DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
	}
	//DlssCreateFeatureFlags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;  // Enable auto exposure

	// Setup create parameters
	NVSDK_NGX_DLSS_Create_Params DlssCreateParams;

	memset(&DlssCreateParams, 0, sizeof(DlssCreateParams));

	DlssCreateParams.Feature.InWidth = optimalRenderSize.x;
	DlssCreateParams.Feature.InHeight = optimalRenderSize.y;
	DlssCreateParams.Feature.InTargetWidth = displayOutSize.x;
	DlssCreateParams.Feature.InTargetHeight = displayOutSize.y;
	DlssCreateParams.Feature.InPerfQualityValue = m_Settings.Quality;
	DlssCreateParams.InFeatureCreateFlags = DlssCreateFeatureFlags;

	// Select render preset (DL weights)
	unsigned int renderPreset = m_Settings.RenderPreset;
	NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, renderPreset);
	NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, renderPreset);
	NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, renderPreset);
	NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, renderPreset);
	NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, renderPreset);

	// Create a command list for feature creation
	GraphicsContext& Context = GraphicsContext::Begin(L"DLSS Init Context");
	ID3D12GraphicsCommandList* commandList = Context.GetCommandList();

	Result = NGX_D3D12_CREATE_DLSS_EXT(commandList, CreationNodeMask, VisibilityNodeMask, &m_dlssFeature, m_ngxParameters, &DlssCreateParams);

	Context.Finish(true);  // Execute and wait for completion

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
		outRecommendedSettings->m_ngxRecommendedSharpness = 0.0f;

		EngineLog::AddLog(L"NGX was not initialized when querying Optimal Settings");
		return false;
	}

	NVSDK_NGX_Result Result = NGX_DLSS_GET_OPTIMAL_SETTINGS(m_ngxParameters,
		inDisplaySize.x, inDisplaySize.y, inQualValue,
		&outRecommendedSettings->m_ngxRecommendedOptimalRenderSize.x, &outRecommendedSettings->m_ngxRecommendedOptimalRenderSize.y,
		&outRecommendedSettings->m_ngxDynamicMaximumRenderSize.x, &outRecommendedSettings->m_ngxDynamicMaximumRenderSize.y,
		&outRecommendedSettings->m_ngxDynamicMinimumRenderSize.x, &outRecommendedSettings->m_ngxDynamicMinimumRenderSize.y,
		&outRecommendedSettings->m_ngxRecommendedSharpness);

	if (NVSDK_NGX_FAILED(Result))
	{
		outRecommendedSettings->m_ngxRecommendedOptimalRenderSize = inDisplaySize;
		outRecommendedSettings->m_ngxDynamicMaximumRenderSize = inDisplaySize;
		outRecommendedSettings->m_ngxDynamicMinimumRenderSize = inDisplaySize;
		outRecommendedSettings->m_ngxRecommendedSharpness = 0.0f;

		EngineLog::AddLog(L"Querying Optimal Settings failed! code = 0x%08x", Result);
		return false;
	}

	EngineLog::AddLog(L"DLSS Optimal Settings - Render Size: %ux%u, Sharpness: %.2f",
		outRecommendedSettings->m_ngxRecommendedOptimalRenderSize.x,
		outRecommendedSettings->m_ngxRecommendedOptimalRenderSize.y,
		outRecommendedSettings->m_ngxRecommendedSharpness);

	return true;
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
			EngineLog::AddLog(L"NVIDIA NGX not available on this hardware/platform., code = 0x%08x", Result);
		else
			EngineLog::AddLog(L"Failed to initialize NGX, error code = 0x%08x", Result);

		return;
	}

	Result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_ngxParameters);

	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"NVSDK_NGX_GetCapabilityParameters failed, code = 0x%08x", Result);
		ShutdownNGX();
		return;
	}

	// Check for driver version requirements
	int needsUpdatedDriver = 0;
	unsigned int minDriverVersionMajor = 0;
	unsigned int minDriverVersionMinor = 0;

	NVSDK_NGX_Result ResultUpdatedDriver = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
	NVSDK_NGX_Result ResultMinDriverMajor = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverVersionMajor);
	NVSDK_NGX_Result ResultMinDriverMinor = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverVersionMinor);

	if (ResultUpdatedDriver == NVSDK_NGX_Result_Success &&
		ResultMinDriverMajor == NVSDK_NGX_Result_Success &&
		ResultMinDriverMinor == NVSDK_NGX_Result_Success)
	{
		if (needsUpdatedDriver)
		{
			EngineLog::AddLog(L"NVIDIA DLSS requires driver update. Minimum version: %u.%u", minDriverVersionMajor, minDriverVersionMinor);
			ShutdownNGX();
			return;
		}
		else
		{
			EngineLog::AddLog(L"NVIDIA DLSS minimum driver version: %u.%u", minDriverVersionMajor, minDriverVersionMinor);
		}
	}

	int dlssAvailable = 0;
	NVSDK_NGX_Result ResultDLSS = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
	if (ResultDLSS != NVSDK_NGX_Result_Success || !dlssAvailable)
	{
		// Get more details about the failure
		NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
		NVSDK_NGX_Parameter_GetI(m_ngxParameters, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, (int*)&FeatureInitResult);
		EngineLog::AddLog(L"NVIDIA DLSS not available. FeatureInitResult = 0x%08x", FeatureInitResult);
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
	DepthBuffer* depthBuffer, ColorBuffer* MotionVectorBuffer,
	const Camera& camera, float deltaTime, bool reset)
{
	if (!IsDLSSInitialized() || !m_Settings.Enable)
	{
		return;
	}

	ScopedTimer DLSSPassScope(L"DLSS", Context);

	// Validate buffer dimensions match what DLSS was initialized with
	uint32_t inputWidth = InputColor.GetWidth();
	uint32_t inputHeight = InputColor.GetHeight();
	uint32_t outputWidth = OutputColor.GetWidth();
	uint32_t outputHeight = OutputColor.GetHeight();

	if (inputWidth != m_OptimalRenderSize.x || inputHeight != m_OptimalRenderSize.y ||
	    outputWidth != m_DisplaySize.x || outputHeight != m_DisplaySize.y)
	{
		EngineLog::AddLog(L"DLSS Execute: Buffer size mismatch!");
		EngineLog::AddLog(L"  Expected Input: %ux%u, Got: %ux%u", m_OptimalRenderSize.x, m_OptimalRenderSize.y, inputWidth, inputHeight);
		EngineLog::AddLog(L"  Expected Output: %ux%u, Got: %ux%u", m_DisplaySize.x, m_DisplaySize.y, outputWidth, outputHeight);
		EngineLog::AddLog(L"  Skipping DLSS this frame.");
		return;
	}

	ID3D12GraphicsCommandList* CommandList = Context.GetCommandList();

	// Transition resources to correct states for DLSS
	Context.TransitionResource(InputColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	if (depthBuffer)
	{
		Context.TransitionResource(*depthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	if (MotionVectorBuffer)
	{
		Context.TransitionResource(*MotionVectorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	Context.TransitionResource(OutputColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// Flush resource barriers
	Context.FlushResourceBarriers();

	// Calculate render subrect dimensions
	NVSDK_NGX_Dimensions renderSubrectDimensions = { InputColor.GetWidth(), InputColor.GetHeight() };
	NVSDK_NGX_Coordinates renderOffset = { 0, 0 };

	NVSDK_NGX_D3D12_DLSS_Eval_Params EvalParams = {};
	EvalParams.Feature.pInColor = InputColor.GetResource();
	EvalParams.Feature.pInOutput = OutputColor.GetResource();

	// Set input textures (required)
	if (depthBuffer && depthBuffer->GetResource())
	{
		EvalParams.pInDepth = depthBuffer->GetResource();
	}

	if (MotionVectorBuffer && MotionVectorBuffer->GetResource())
	{
		EvalParams.pInMotionVectors = MotionVectorBuffer->GetResource();
	}

	// Set evaluation parameters
	EvalParams.InJitterOffsetX = -m_JitterOffset.x;
	EvalParams.InJitterOffsetY = -m_JitterOffset.y;
	// Use either explicit reset or accumulated reset request
	EvalParams.InReset = (reset || ConsumeResetAccumulation()) ? 1 : 0;
	EvalParams.InMVScaleX = inputWidth;
	EvalParams.InMVScaleY = inputHeight;
	EvalParams.Feature.InSharpness = m_Settings.EnableSharpening ? m_Settings.Sharpness : 0.0f;

	// Set subrect parameters
	EvalParams.InRenderSubrectDimensions = renderSubrectDimensions;
	EvalParams.InColorSubrectBase = renderOffset;
	EvalParams.InDepthSubrectBase = renderOffset;
	EvalParams.InMVSubrectBase = renderOffset;
	EvalParams.InTranslucencySubrectBase = renderOffset;

	NVSDK_NGX_Result Result = NGX_D3D12_EVALUATE_DLSS_EXT(CommandList, m_dlssFeature, m_ngxParameters, &EvalParams);

	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"DLSS EvaluateFeature failed, code = 0x%08x", Result);
		EngineLog::AddLog(L"  Input: %ux%u, Output: %ux%u, RenderSize: %ux%u, DisplaySize: %ux%u",
			InputColor.GetWidth(), InputColor.GetHeight(),
			OutputColor.GetWidth(), OutputColor.GetHeight(),
			m_OptimalRenderSize.x, m_OptimalRenderSize.y,
			m_DisplaySize.x, m_DisplaySize.y);
		EngineLog::AddLog(L"  Depth: %p, MotionVectors: %p",
			depthBuffer ? depthBuffer->GetResource() : nullptr,
			MotionVectorBuffer ? MotionVectorBuffer->GetResource() : nullptr);
	}
}

void DLSS::UpdateJitter(int frameIndex)
{
	if (!IsDLSSInitialized() || !m_Settings.Enable || m_OptimalRenderSize.x == 0 || m_OptimalRenderSize.y == 0)
	{
		m_JitterOffset = XMFLOAT2(0, 0);
		return;
	}

	XMFLOAT2 Result(0.0f, 0.0f);

	frameIndex = frameIndex % NUM_OFFSET_SEQUENCES;

	// Halton jitter
	constexpr int BaseX = 2;
	int Index = frameIndex + 1;
	float InvBase = 1.0f / BaseX;
	float Fraction = InvBase;
	while (Index > 0)
	{
		Result.x += (Index % BaseX) * Fraction;
		Index /= BaseX;
		Fraction *= InvBase;
	}

	constexpr int BaseY = 3;
	Index = frameIndex + 1;
	InvBase = 1.0f / BaseY;
	Fraction = InvBase;
	while (Index > 0)
	{
		Result.y += (Index % BaseY) * Fraction;
		Index /= BaseY;
		Fraction *= InvBase;
	}

	Result.x -= 0.5f;
	Result.y -= 0.5f;

	m_JitterOffset = Result;
}

void DLSS::UpdateUpscaleRatio()
{
	if (m_OptimalRenderSize.x > 0 && m_DisplaySize.x > 0)
	{
		m_UpscaleRatio = static_cast<float>(m_DisplaySize.x) / static_cast<float>(m_OptimalRenderSize.x);
		m_MipBias = CalculateMipBias(m_UpscaleRatio);
	}
	else
	{
		m_UpscaleRatio = 1.0f;
		m_MipBias = 0.0f;
	}
}

float DLSS::CalculateMipBias(float upscalerRatio) const
{
	return std::log2f(1.0f / upscalerRatio) - 1.0f + std::numeric_limits<float>::epsilon();
}

float DLSS::GetMipBias() const
{
	if (!m_Settings.Enable || !IsDLSSInitialized())
		return 0.0f;
	return m_MipBias;
}

float DLSS::GetUpscaleRatio() const
{
	if (!m_Settings.Enable || !IsDLSSInitialized())
		return 1.0f;
	return m_UpscaleRatio;
}

bool DLSS::NeedsRecreate(NVSDK_NGX_PerfQuality_Value newQuality) const
{
	return newQuality != m_Settings.Quality;
}

void DLSS::SetDLSSEnabled(bool enabled)
{
	if (m_Settings.Enable != enabled)
	{
		m_Settings.Enable = enabled;
		if (!enabled)
		{
			ResetJitter();
		}
	}
}

void DLSS::SetQuality(NVSDK_NGX_PerfQuality_Value quality)
{
	if (m_Settings.Quality != quality && IsDLSSInitialized())
	{
		m_Settings.Quality = quality;
		m_NeedsRecreate = true;
		Display::g_bDLSSUpscale = true;
	}
	else
	{
		m_Settings.Quality = quality;
	}
}

void DLSS::SetRenderPreset(NVSDK_NGX_DLSS_Hint_Render_Preset preset)
{
	if (m_Settings.RenderPreset != preset && IsDLSSInitialized())
	{
		m_Settings.RenderPreset = preset;
		m_NeedsRecreate = true;
	}
	else
	{
		m_Settings.RenderPreset = preset;
	}
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

	if (m_Settings.Enable && IsDLSSInitialized())
	{
		ImGui::Text("Render Resolution: %ux%u", m_OptimalRenderSize.x, m_OptimalRenderSize.y);
		ImGui::Text("Output Resolution: %ux%u", m_DisplaySize.x, m_DisplaySize.y);
		ImGui::Text("Upscale Ratio: %.2fx", m_UpscaleRatio);
		ImGui::Text("Mip Bias: %.2f", m_MipBias);
		ImGui::Text("Sharpness: %.2f", m_Settings.Sharpness);

		// Quality names in the same order as NVSDK_NGX_PerfQuality_Value enum
		const char* qualityNames[] = { "Performance", "Balanced", "Quality", "Ultra Performance", "DLAA (Native AA)" };
		int currentQuality = 0;
		switch (m_Settings.Quality)
		{
		case NVSDK_NGX_PerfQuality_Value_MaxPerf:          currentQuality = 0; break;
		case NVSDK_NGX_PerfQuality_Value_Balanced:         currentQuality = 1; break;
		case NVSDK_NGX_PerfQuality_Value_MaxQuality:       currentQuality = 2; break;
		case NVSDK_NGX_PerfQuality_Value_UltraPerformance: currentQuality = 3; break;
		case NVSDK_NGX_PerfQuality_Value_DLAA:             currentQuality = 4; break;
		default: currentQuality = 0; break;
		}

		int previousQuality = currentQuality;
		if (ImGui::Combo("DLSS Quality", &currentQuality, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			if (currentQuality != previousQuality)
			{
				switch (currentQuality)
				{
				case 0: SetQuality(NVSDK_NGX_PerfQuality_Value_MaxPerf); break;
				case 1: SetQuality(NVSDK_NGX_PerfQuality_Value_Balanced); break;
				case 2: SetQuality(NVSDK_NGX_PerfQuality_Value_MaxQuality); break;
				case 3: SetQuality(NVSDK_NGX_PerfQuality_Value_UltraPerformance); break;
				case 4: SetQuality(NVSDK_NGX_PerfQuality_Value_DLAA); break;
				}
			}
		}

		// Handle automatic recreation when quality changes
		if (m_NeedsRecreate && IsDLSSInitialized())
		{
			// Re-query optimal settings for new quality
			DLSSRecommendedSettings recommended;
			if (QueryOptimalSettings(m_DisplaySize, m_Settings.Quality, &recommended))
			{
				ReleaseDLSSFeatures();
				m_OptimalRenderSize = recommended.m_ngxRecommendedOptimalRenderSize;
				CreateDLSSFeature(m_OptimalRenderSize, m_DisplaySize);
				UpdateUpscaleRatio();
				m_NeedsRecreate = false;
				EngineLog::AddLog(L"DLSS quality changed - Recreated feature with render size: %ux%u",
					m_OptimalRenderSize.x, m_OptimalRenderSize.y);
			}
		}

		// Render Preset selection
		const char* presetNames[] = { "Default", "F (UltraPerf/DLAA)", "J (Less Ghosting)", "K (Transformer, Best Quality)" };
		unsigned int presetValues[] = {
			NVSDK_NGX_DLSS_Hint_Render_Preset_Default,
			NVSDK_NGX_DLSS_Hint_Render_Preset_F,
			NVSDK_NGX_DLSS_Hint_Render_Preset_J,
			NVSDK_NGX_DLSS_Hint_Render_Preset_K
		};
		int currentPresetIndex = 0;
		for (int i = 0; i < IM_ARRAYSIZE(presetValues); i++)
		{
			if (presetValues[i] == m_Settings.RenderPreset)
			{
				currentPresetIndex = i;
				break;
			}
		}
		int previousPresetIndex = currentPresetIndex;
		if (ImGui::Combo("Render Preset", &currentPresetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
		{
			if (currentPresetIndex != previousPresetIndex)
			{
				SetRenderPreset((NVSDK_NGX_DLSS_Hint_Render_Preset)presetValues[currentPresetIndex]);
			}
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::Text("K: Transformer model, best quality (recommended)\nF: For Ultra Performance/DLAA modes\nJ: Less ghosting, more flickering than K");
			ImGui::EndTooltip();
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
			ResetJitter();
		}
	}
}
