#pragma once

#include <cmath>
#include <limits> 


#include "ffx-api/ffx_api.hpp"
#include "ffx-api/ffx_upscale.hpp"
#include "ffx-api/ffx_framegeneration.hpp"

class FSR
{
public:
	FSR() {}

	~FSR();

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

private:
	enum class FSRScalePreset
	{
		NativeAA = 0,       // 1.0f
		Quality,            // 1.5f
		Balanced,           // 1.7f
		Performance,        // 2.0f
		UltraPerformance,   // 3.0f
		Custom              // 1.0f - 3.0f range
	};

	const float cMipBias[static_cast<uint32_t>(FSRScalePreset::Custom)] = {
	   std::log2f(1.0f / 1.0f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 1.5f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 1.7f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 2.0f) - 1.0f + std::numeric_limits<float>::epsilon(),
	   std::log2f(1.0f / 3.0f) - 1.0f + std::numeric_limits<float>::epsilon()
	};


private:
	bool m_UpscalerEnabled = false;
	bool m_FrameInterpolationEnabled = false;

	FSRScalePreset  m_ScalePreset = FSRScalePreset::Quality;


};

