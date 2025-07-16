#pragma once

#include <cmath>
#include <limits> 
#include <DirectXMath.h>

#include "ffx-api/ffx_api.hpp"
#include "ffx-api/ffx_upscale.hpp"
#include "ffx-api/ffx_framegeneration.hpp"

class ID3D12GraphicsCommandList;

class FSR
{
public:
	static FSR* GetInstance();
	static void Shutdown();

	void Initialize();

	bool UpscalerEnabled() 
	{
		return m_UpscalerEnabled;
	}
	void EnableUpscaling(bool enabled) 
	{
		m_UpscalerEnabled = enabled;
	}

	bool FrameInterpolationEnabled()
	{
		return m_FrameInterpolationEnabled;
	}
	void EnableFrameInterpolation(bool enabled)
	{
		m_FrameInterpolationEnabled = enabled;
	}

	void DrawUI();

	static void FSRMsgCallback(uint32_t type, const wchar_t* message);

	float GetMipLODBias() { return m_MipBias; }

	void  SetMipLODBias(float pBias) { m_MipBias = pBias; }

	float GetUpscaleRatio() { return m_UpscaleRatio; }

	void Execute(double deltaTime, ID3D12GraphicsCommandList* pCmdList);

	void GetFSRjitter(DirectX::XMFLOAT2& jitter);

	void ResetJitterIndex() { m_JitterIndex = 0; }
private:
	FSR() {}
	~FSR();

	enum class FSRScalePreset
	{
		NativeAA = 0,       // 1.0f
		Quality,            // 1.5f
		Balanced,           // 1.7f
		Performance,        // 2.0f
		UltraPerformance,   // 3.0f
		Count               
	};

	enum FSRDebugCheckerMode
	{
		Disabled = 0,
		EnabledNoMessageCallback,
		EnabledWithMessageCallback
	};

	const float cMipBias[static_cast<uint32_t>(FSRScalePreset::Count)] = {
	   std::log2f(1.0f / 1.0f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 1.5f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 1.7f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 2.0f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 3.0f) - 1.0f + std::numeric_limits<float>::epsilon()
	};

	inline float CalculateMipBias(float upscalerRatio)
	{
		return std::log2f(1.f / upscalerRatio) - 1.f + std::numeric_limits<float>::epsilon();
	}

	void UpdateUpscalerPreset(const int32_t pNewPreset);

	void SetGlobalDebugCheckerMode(FSRDebugCheckerMode mode);

private:
	static FSR* m_pFSRInstance;

	bool m_UpscalerEnabled = false;
	bool m_FrameInterpolationEnabled = false;
	bool m_UseMask = true;

	FSRDebugCheckerMode      m_GlobalDebugCheckerMode = FSRDebugCheckerMode::EnabledWithMessageCallback;

	FSRScalePreset  m_ScalePreset = FSRScalePreset::NativeAA;
	int32_t			m_NewPreset = 0;
	float           m_MipBias = 0;

	float           m_UpscaleRatio = 1.0f;
	float           m_Sharpness = 0.8f;
	uint32_t        m_JitterIndex = 0;
	float           m_JitterX = 0.f;
	float           m_JitterY = 0.f;


	bool m_ffxBackendInitialized = false;
	ffx::Context m_UpscalingContext = nullptr;
	ffx::Context m_FrameGenContext = nullptr;
	ffx::Context m_SwapChainContext = nullptr;
	ffx::ConfigureDescFrameGeneration m_FrameGenerationConfig{};

	uint64_t    m_CurrentUpscaleContextVersionId = 0;
	const char* m_CurrentUpscaleContextVersionName = nullptr;
};

