#include "FSR.h"
#include "Engine/EngineLog.h"
#include "Graphics/CommandContext.h"
#include "imgui.h"
#include "Engine/GameCore.h"
#include "BufferManager.h"
#include "EngineGUI.h"



FSR::~FSR()
{
}

void FSR::DrawUI()
{
	if (ImGui::CollapsingHeader("FSR", ImGuiTreeNodeFlags_None))
	{
		ImGui::Combo("FSR Quality", &m_NewPreset, "NativeAA\0Quality\0Balanced\0Performance\0UltraPerformance");
		if (m_NewPreset != (int32_t)m_ScalePreset)
		{
			UpdateUpscalerPreset(m_NewPreset);
		}
	}
}

void FSR::FSRMsgCallback(uint32_t type, const wchar_t* message)
{
	if (type == FFX_API_MESSAGE_TYPE_ERROR)
	{
		EngineLog::AddLog(L"FSR_API_DEBUG_ERROR: %ls", message);
	}
	else if (type == FFX_API_MESSAGE_TYPE_WARNING)
	{
		EngineLog::AddLog(L"FSR_API_DEBUG_WARNING: %ls", message);
	}
}

void FSR::UpdateUpscalerPreset(const int32_t NewPreset)
{
	m_ScalePreset = FSRScalePreset(NewPreset);
	switch (m_ScalePreset)
	{
	case FSRScalePreset::NativeAA:
		m_UpscaleRatio = 1.0f;
		break;
	case FSRScalePreset::Quality:
		m_UpscaleRatio = 1.5f;
		break;
	case FSRScalePreset::Balanced:
		m_UpscaleRatio = 1.7f;
		break;
	case FSRScalePreset::Performance:
		m_UpscaleRatio = 2.0f;
		break;
	case FSRScalePreset::UltraPerformance:
		m_UpscaleRatio = 3.0f;
		break;
	default:
		break;
	}

	// Update Mip bias
	m_MipBias = cMipBias[static_cast<uint32_t>(m_ScalePreset)];

	Display::g_UpscaleRatio = m_UpscaleRatio;

	Display::g_bUpscale = true;
}
