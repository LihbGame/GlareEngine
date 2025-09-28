#pragma once

#include <cmath>
#include <limits> 
#include <DirectXMath.h>
#include "Graphics/ColorBuffer.h"

#include "FidelityFX/ffx-api/ffx_api_types.h"
#include "FidelityFX/ffx-api/ffx_api.hpp"
#include "FidelityFX/ffx-api/ffx_upscale.hpp"
#include "FidelityFX/ffx-api/ffx_framegeneration.hpp"

class ID3D12GraphicsCommandList;

class FSR
{
public:
	static FSR* GetInstance();
	static void Shutdown();

	void UpdateUpscalingContext(bool enable);

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

	void Execute(ComputeContext& Context,ColorBuffer& Input,ColorBuffer& Output);

	DirectX::XMFLOAT2 GetFSRjitter();

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

	/*inline FfxSurfaceFormat GetFfxSurfaceFormat(DXGI_FORMAT format)
	{
		switch (format)
		{
		case (DXGI_FORMAT_R32G32B32A32_TYPELESS):
			return FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
		case (cauldron::ResourceFormat::RGBA32_UINT):
			return FFX_SURFACE_FORMAT_R32G32B32A32_UINT;
		case (cauldron::ResourceFormat::RGBA32_FLOAT):
			return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
		case (cauldron::ResourceFormat::RGBA16_TYPELESS):
			return FFX_SURFACE_FORMAT_R16G16B16A16_TYPELESS;
		case (cauldron::ResourceFormat::RGBA16_FLOAT):
			return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
		case (cauldron::ResourceFormat::RGB32_FLOAT):
			return FFX_SURFACE_FORMAT_R32G32B32_FLOAT;
		case (cauldron::ResourceFormat::RG32_TYPELESS):
			return FFX_SURFACE_FORMAT_R32G32_TYPELESS;
		case (cauldron::ResourceFormat::RG32_FLOAT):
			return FFX_SURFACE_FORMAT_R32G32_FLOAT;
		case (cauldron::ResourceFormat::R8_UINT):
			return FFX_SURFACE_FORMAT_R8_UINT;
		case (cauldron::ResourceFormat::R32_UINT):
			return FFX_SURFACE_FORMAT_R32_UINT;
		case (cauldron::ResourceFormat::RGBA8_TYPELESS):
			return FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
		case (cauldron::ResourceFormat::RGBA8_UNORM):
			return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
		case (cauldron::ResourceFormat::RGBA8_SNORM):
			return FFX_SURFACE_FORMAT_R8G8B8A8_SNORM;
		case (cauldron::ResourceFormat::RGBA8_SRGB):
			return FFX_SURFACE_FORMAT_R8G8B8A8_SRGB;
		case (cauldron::ResourceFormat::BGRA8_TYPELESS):
			return FFX_SURFACE_FORMAT_B8G8R8A8_TYPELESS;
		case (cauldron::ResourceFormat::BGRA8_UNORM):
			return FFX_SURFACE_FORMAT_B8G8R8A8_UNORM;
		case (cauldron::ResourceFormat::BGRA8_SRGB):
			return FFX_SURFACE_FORMAT_B8G8R8A8_SRGB;
		case (cauldron::ResourceFormat::RG11B10_FLOAT):
			return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
		case (cauldron::ResourceFormat::RGB9E5_SHAREDEXP):
			return FFX_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP;
		case (cauldron::ResourceFormat::RGB10A2_UNORM):
			return FFX_SURFACE_FORMAT_R10G10B10A2_UNORM;
		case (cauldron::ResourceFormat::RGB10A2_TYPELESS):
			return FFX_SURFACE_FORMAT_R10G10B10A2_TYPELESS;
		case (cauldron::ResourceFormat::RG16_TYPELESS):
			return FFX_SURFACE_FORMAT_R16G16_TYPELESS;
		case (cauldron::ResourceFormat::RG16_FLOAT):
			return FFX_SURFACE_FORMAT_R16G16_FLOAT;
		case (cauldron::ResourceFormat::RG16_UINT):
			return FFX_SURFACE_FORMAT_R16G16_UINT;
		case (cauldron::ResourceFormat::RG16_SINT):
			return FFX_SURFACE_FORMAT_R16G16_SINT;
		case (cauldron::ResourceFormat::R16_TYPELESS):
			return FFX_SURFACE_FORMAT_R16_TYPELESS;
		case (cauldron::ResourceFormat::R16_FLOAT):
			return FFX_SURFACE_FORMAT_R16_FLOAT;
		case (cauldron::ResourceFormat::R16_UINT):
			return FFX_SURFACE_FORMAT_R16_UINT;
		case (cauldron::ResourceFormat::R16_UNORM):
			return FFX_SURFACE_FORMAT_R16_UNORM;
		case (cauldron::ResourceFormat::R16_SNORM):
			return FFX_SURFACE_FORMAT_R16_SNORM;
		case (cauldron::ResourceFormat::R8_TYPELESS):
			return FFX_SURFACE_FORMAT_R8_TYPELESS;
		case (cauldron::ResourceFormat::R8_UNORM):
			return FFX_SURFACE_FORMAT_R8_UNORM;
		case cauldron::ResourceFormat::RG8_TYPELESS:
			return FFX_SURFACE_FORMAT_R8G8_TYPELESS;
		case cauldron::ResourceFormat::RG8_UNORM:
			return FFX_SURFACE_FORMAT_R8G8_UNORM;
		case cauldron::ResourceFormat::RG8_UINT:
			return FFX_SURFACE_FORMAT_R8G8_UINT;
		case cauldron::ResourceFormat::R32_TYPELESS:
			return FFX_SURFACE_FORMAT_R32_TYPELESS;
		case cauldron::ResourceFormat::R32_FLOAT:
		case cauldron::ResourceFormat::D32_FLOAT:
			return FFX_SURFACE_FORMAT_R32_FLOAT;
		case (cauldron::ResourceFormat::Unknown):
			return FFX_SURFACE_FORMAT_UNKNOWN;
		default:
			EngineLog::AddLog(L"ValidationRemap: Unsupported format requested. Please implement.");
			return FFX_SURFACE_FORMAT_UNKNOWN;
		}
	}*/
private:
	static FSR* m_pFSRInstance;

	bool m_UpscalerEnabled = true;
	bool m_FrameInterpolationEnabled = false;
	bool m_UseMask = true;
	bool m_DrawFrameGenerationDebugView = false;
	bool m_EnableAsyncCompute = true;
	bool m_DrawUpscalerDebugView = false;
	bool m_ResetUpscale = false;
	bool m_SharpnessEnabled = true;

	FSRDebugCheckerMode      m_GlobalDebugCheckerMode = FSRDebugCheckerMode::EnabledWithMessageCallback;

	FSRScalePreset  m_ScalePreset = FSRScalePreset::Count;
	int32_t			m_NewPreset = 0;
	float           m_MipBias = 0;

	float           m_UpscaleRatio = 1.0f;
	float           m_Sharpness = 1.0f;
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

