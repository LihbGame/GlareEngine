#pragma once
#include <memory>
#include <DirectXMath.h>
#include <xstring>
#include "Math/Common.h"
#include "Graphics/ColorBuffer.h"
#include "Graphics/DepthBuffer.h"

#include "DLSS/nvsdk_ngx.h"
#include "DLSS/nvsdk_ngx_helpers.h"
#include "Engine/Camera.h"

#define APP_ID 231313132

class DLSS
{
public:
    struct DLSSRecommendedSettings
    {
        float        m_ngxRecommendedSharpness = 0.0f;
        Math::UINT2  m_ngxRecommendedOptimalRenderSize;
        Math::UINT2  m_ngxDynamicMaximumRenderSize;
        Math::UINT2  m_ngxDynamicMinimumRenderSize;
    };

    struct DLSSSettings
    {
        bool Enable = true;
        bool EnableSharpening = false;
        float Sharpness = 0.0f;
        NVSDK_NGX_PerfQuality_Value Quality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
    };

    static DLSS* GetInstance();
    static void Shutdown();

    bool InitializeDLSSFeatures(Math::UINT2 optimalRenderSize, Math::UINT2 displayOutSize, const DLSSSettings& settings);
    void ReleaseDLSSFeatures();

    bool QueryOptimalSettings(Math::UINT2 inDisplaySize, NVSDK_NGX_PerfQuality_Value inQualValue, DLSSRecommendedSettings* outRecommendedSettings);

    bool IsDLSSAvailable() const { return m_bDlssAvailable; }
    bool IsDLSSInitialized() const { return IsNGXInitialized() && m_dlssFeature != nullptr; }
    bool IsNGXInitialized() const { return m_ngxInitialized; }

    void Execute(ComputeContext& Context, ColorBuffer& InputColor, ColorBuffer& OutputColor, 
                 DepthBuffer* DepthBuffer, ColorBuffer* MotionVectorBuffer, 
                 const Camera& camera, float deltaTime, bool reset);

    void UpdateJitter(int frameIndex);
    XMFLOAT2 GetJitterOffset() const { return m_JitterOffset; }
    void ResetJitter() { m_JitterIndex = 0; m_JitterOffset = XMFLOAT2(0, 0); }

    void DrawUI();

    void SetDLSSEnabled(bool enabled) { m_Settings.Enable = enabled; }
    bool IsDLSSEnabled() const { return m_Settings.Enable; }

    void SetQuality(NVSDK_NGX_PerfQuality_Value quality) { m_Settings.Quality = quality; }
    NVSDK_NGX_PerfQuality_Value GetQuality() const { return m_Settings.Quality; }

    void SetSharpness(float sharpness) { m_Settings.Sharpness = sharpness; }
    float GetSharpness() const { return m_Settings.Sharpness; }

    void SetEnableSharpening(bool enabled) { m_Settings.EnableSharpening = enabled; }
    bool IsSharpeningEnabled() const { return m_Settings.EnableSharpening; }

    Math::UINT2 GetOptimalRenderSize() const { return m_OptimalRenderSize; }
    Math::UINT2 GetDisplaySize() const { return m_DisplaySize; }

private:
    DLSS();
    ~DLSS();

    static DLSS* m_pDLSSInstance;

    bool m_ngxInitialized = false;
    bool m_bDlssAvailable = false;

    NVSDK_NGX_Parameter* m_ngxParameters = nullptr;
    NVSDK_NGX_Handle* m_dlssFeature = nullptr;

    DLSSSettings m_Settings;
    Math::UINT2 m_OptimalRenderSize = { 0, 0 };
    Math::UINT2 m_DisplaySize = { 0, 0 };

    int m_JitterIndex = 0;
    XMFLOAT2 m_JitterOffset = { 0, 0 };

    void InitializeNGX(const wchar_t* rwLogsFolder = L".");
    void ShutdownNGX();

    IDXGIAdapter* m_Adapter{};
    bool FindAdapter(const std::wstring& targetName);

    void CreateDLSSFeature(Math::UINT2 optimalRenderSize, Math::UINT2 displayOutSize);
};