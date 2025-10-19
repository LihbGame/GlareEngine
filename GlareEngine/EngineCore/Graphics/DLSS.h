#pragma once
#include <memory>
#include <DirectXMath.h>
#include "DLSS/nvsdk_ngx.h"

#define APP_ID 123456789

class DLSS
{
public:
	struct DLSSRecommendedSettings
	{
		float     m_ngxRecommendedSharpness = 0.01f; // in ngx sdk 3.1, dlss sharpening is deprecated
		DirectX::XMFLOAT2  m_ngxRecommendedOptimalRenderSize;
		DirectX::XMFLOAT2  m_ngxDynamicMaximumRenderSize;
		DirectX::XMFLOAT2  m_ngxDynamicMinimumRenderSize;
	};

	static DLSS* GetInstance();
	static void Shutdown();

	bool InitializeDLSSFeatures(DirectX::XMFLOAT2 optimalRenderSize, DirectX::XMFLOAT2 displayOutSize, int isContentHDR, bool depthInverted, float depthScale = 1.0f, bool enableSharpening = false, bool enableAutoExposure = false, NVSDK_NGX_PerfQuality_Value qualValue = NVSDK_NGX_PerfQuality_Value_MaxPerf, NVSDK_NGX_DLSS_Hint_Render_Preset renderPreset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
	void ReleaseDLSSFeatures();

	bool QueryOptimalSettings(DirectX::XMFLOAT2 inDisplaySize, NVSDK_NGX_PerfQuality_Value inQualValue, DLSSRecommendedSettings* outRecommendedSettings);

	bool IsDLSSAvailable() const { return m_bDlssAvailable; }

	bool IsDLSSInitialized() const { return IsNGXInitialized() && m_dlssFeature != nullptr; }
	bool IsNGXInitialized() const { return m_ngxInitialized; }

	bool IsFeatureSupported(NVSDK_NGX_FeatureDiscoveryInfo* dis);
private:
	DLSS() {}
	~DLSS();

private:
	static DLSS* m_pDLSSInstance;

	bool m_ngxInitialized = false;
	bool m_bDlssAvailable = false;

	NVSDK_NGX_Parameter* m_ngxParameters = nullptr;
	NVSDK_NGX_Handle* m_dlssFeature = nullptr;

	void InitializeNGX(const wchar_t* rwLogsFolder = L".");
	void ShutdownNGX();

};

