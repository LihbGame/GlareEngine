#pragma once
#include <memory>
#include <cmath>
#include <limits>
#include <DirectXMath.h>
#include <xstring>
#include "Math/Common.h"
#include "Graphics/ColorBuffer.h"
#include "Graphics/DepthBuffer.h"
#include "Graphics/Display.h"

#include "DLSS/nvsdk_ngx.h"
#include "DLSS/nvsdk_ngx_helpers.h"
#include "Engine/Camera.h"

#define APP_ID 331313132

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
        bool EnableSharpening = true;
        float Sharpness = 0.0f;
        NVSDK_NGX_PerfQuality_Value Quality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
        NVSDK_NGX_DLSS_Hint_Render_Preset RenderPreset = NVSDK_NGX_DLSS_Hint_Render_Preset_K;
    };

    // Quality mode presets and their scale ratios
    enum class QualityPreset
    {
        DLAA = 0,           // 1.0x (Native quality anti-aliasing)
        Quality,            // 1.5x
        Balanced,           // 1.7x
        Performance,        // 2.0x
        UltraPerformance,   // 3.0x
        Native,             // 1.0x (No upscaling)
        Count
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
                 DepthBuffer* depthBuffer, ColorBuffer* MotionVectorBuffer,
                 const Camera& camera, float deltaTime, bool reset);

    void UpdateJitter(int frameIndex);
    XMFLOAT2 GetJitterOffset() const { return XMFLOAT2(m_JitterOffset.x / Display::g_RenderWidth, m_JitterOffset.y / Display::g_RenderHeight); }
    void ResetJitter() { m_JitterIndex = 0; m_JitterOffset = XMFLOAT2(0, 0); }

    // Mip bias for texture LOD when using DLSS
    float GetMipBias() const;
    float GetUpscaleRatio() const;

    void DrawUI();

    void SetDLSSEnabled(bool enabled);
    bool IsDLSSEnabled() const { return m_Settings.Enable; }

    void SetQuality(NVSDK_NGX_PerfQuality_Value quality);
    NVSDK_NGX_PerfQuality_Value GetQuality() const { return m_Settings.Quality; }

    // Reset accumulation on scene changes or camera cuts
    void RequestResetAccumulation() { m_NeedsResetAccumulation = true; }
    bool ConsumeResetAccumulation()
    {
        bool reset = m_NeedsResetAccumulation;
        m_NeedsResetAccumulation = false;
        return reset;
    }

    void SetSharpness(float sharpness) { m_Settings.Sharpness = sharpness; }
    float GetSharpness() const { return m_Settings.Sharpness; }

    void SetEnableSharpening(bool enabled) { m_Settings.EnableSharpening = enabled; }
    bool IsSharpeningEnabled() const { return m_Settings.EnableSharpening; }

    void SetRenderPreset(NVSDK_NGX_DLSS_Hint_Render_Preset preset);
    NVSDK_NGX_DLSS_Hint_Render_Preset GetRenderPreset() const { return m_Settings.RenderPreset; }

    Math::UINT2 GetOptimalRenderSize() const { return m_OptimalRenderSize; }
    Math::UINT2 GetDisplaySize() const { return m_DisplaySize; }

    // Check if quality mode change requires feature recreation
    bool NeedsRecreate(NVSDK_NGX_PerfQuality_Value newQuality) const;

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

    // Mip bias cache
    float m_MipBias = 0.0f;
    float m_UpscaleRatio = 1.0f;

    // Track if we need to recreate the feature
    bool m_NeedsRecreate = false;

    // Track if we need to reset accumulation (scene change, camera cut)
    bool m_NeedsResetAccumulation = false;

    void InitializeNGX(const wchar_t* rwLogsFolder = L".");
    void ShutdownNGX();

    IDXGIAdapter* m_Adapter{};
    bool FindAdapter(const std::wstring& targetName);

    void CreateDLSSFeature(Math::UINT2 optimalRenderSize, Math::UINT2 displayOutSize);

    void UpdateUpscaleRatio();
    float CalculateMipBias(float upscalerRatio) const;
};