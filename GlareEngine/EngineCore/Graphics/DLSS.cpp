#include "DLSS.h"
#include "DLSS/nvsdk_ngx_helpers.h"
#include "Engine/EngineLog.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"

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

DLSS::~DLSS()
{

}

void DLSS::InitializeNGX(const wchar_t* rwLogsFolder)
{
	NVSDK_NGX_Result Result = NVSDK_NGX_Result_Fail;

	Result = NVSDK_NGX_D3D12_Init(APP_ID, rwLogsFolder, g_Device);

	m_ngxInitialized = !NVSDK_NGX_FAILED(Result);
	if (!IsNGXInitialized())
	{
		if (Result == NVSDK_NGX_Result_FAIL_FeatureNotSupported || Result == NVSDK_NGX_Result_FAIL_PlatformError)
			EngineLog::AddLog(L"NVIDIA NGX not available on this hardware/platform., code = 0x%08x, info: %ls", Result, GetNGXResultAsString(Result));
		else
			EngineLog::AddLog(L"Failed to initialize NGX, error code = 0x%08x, info: %ls", Result, GetNGXResultAsString(Result));

		return;
	}

	Result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_ngxParameters);

	if (NVSDK_NGX_FAILED(Result))
	{
		EngineLog::AddLog(L"NVSDK_NGX_GetCapabilityParameters failed, code = 0x%08x, info: %ls", Result, GetNGXResultAsString(Result));
		ShutdownNGX();
		return;
	}

// Currently, the SDK and this sample are not in sync.  The sample is a bit forward looking,
// in this case.  This will likely be resolved very shortly, and therefore, the code below
// should be thought of as needed for a smooth user experience.
#if defined(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver)        \
    && defined (NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor) \
    && defined (NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor)

	// If NGX Successfully initialized then it should set those flags in return
	int needsUpdatedDriver = 0;
	unsigned int minDriverVersionMajor = 0;
	unsigned int minDriverVersionMinor = 0;
	NVSDK_NGX_Result ResultUpdatedDriver = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
	NVSDK_NGX_Result ResultMinDriverVersionMajor = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &minDriverVersionMajor);
	NVSDK_NGX_Result ResultMinDriverVersionMinor = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &minDriverVersionMinor);
	if (ResultUpdatedDriver == NVSDK_NGX_Result_Success &&
		ResultMinDriverVersionMajor == NVSDK_NGX_Result_Success &&
		ResultMinDriverVersionMinor == NVSDK_NGX_Result_Success)
	{
		if (needsUpdatedDriver)
		{
			EngineLog::AddLog(L"NVIDIA DLSS cannot be loaded due to outdated driver. Minimum Driver Version required : %u.%u", minDriverVersionMajor, minDriverVersionMinor);

			ShutdownNGX();
			return;
		}
		else
		{
			EngineLog::AddLog(L"NVIDIA DLSS Minimum driver version was reported as : %u.%u", minDriverVersionMajor, minDriverVersionMinor);
		}
	}
	else
	{
		EngineLog::AddLog(L"NVIDIA DLSS Minimum driver version was not reported.");
	}

#endif

	int dlssAvailable = 0;
	NVSDK_NGX_Result ResultDLSS = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &dlssAvailable);
	if (ResultDLSS != NVSDK_NGX_Result_Success || !dlssAvailable)
	{
		// More details about what failed (per feature init result)
		NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
		NVSDK_NGX_Parameter_GetI(m_ngxParameters, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, (int*)&FeatureInitResult);
		EngineLog::AddLog(L"NVIDIA DLSS not available on this hardware/platform., FeatureInitResult = 0x%08x, info: %ls", FeatureInitResult, GetNGXResultAsString(FeatureInitResult));
		ShutdownNGX();
		return;
	}
}

void DLSS::ShutdownNGX()
{
	if (IsNGXInitialized())
	{
		g_CommandManager.IdleGPU();

		if (m_dlssFeature != nullptr)
		{
			EngineLog::AddLog(L"Attempt to release NGX library before features have been released!  Releasing now but should check your code.");
			ReleaseDLSSFeatures();
		}

		NVSDK_NGX_D3D12_DestroyParameters(m_ngxParameters);
		NVSDK_NGX_D3D12_Shutdown1(nullptr);

		m_ngxInitialized = false;
	}
}
