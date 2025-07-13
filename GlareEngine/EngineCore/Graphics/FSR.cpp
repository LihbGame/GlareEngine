#include "FSR.h"
#include "Engine/EngineLog.h"
#include "Graphics/CommandContext.h"
#include "imgui.h"
#include "Engine/GameCore.h"
#include "BufferManager.h"
#include "EngineGUI.h"

#include "ffx-api/dx12/ffx_api_dx12.hpp"

FSR* FSR::m_pFSRInstance = nullptr;

FSR::~FSR()
{
	if (m_UpscalingContext)
	{
		ffx::DestroyContext(m_UpscalingContext);
		m_UpscalingContext = nullptr;
	}
}

FSR* FSR::GetInstance()
{
	if (m_pFSRInstance == nullptr)
	{
		m_pFSRInstance = new FSR();
	}
	return m_pFSRInstance;
}

void FSR::Shutdown()
{
	if (m_pFSRInstance)
	{
		delete m_pFSRInstance;
		m_pFSRInstance = nullptr;
	}
}

void FSR::Initialize()
{
	ffx::CreateBackendDX12Desc backendDesc{};
	backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
	backendDesc.device = g_Device;

	ffx::CreateContextDescUpscale createFsr{};

	createFsr.maxUpscaleSize = { Display::g_DisplayWidth, Display::g_DisplayHeight };
	createFsr.maxRenderSize = { Display::g_DisplayWidth, Display::g_DisplayHeight };

	createFsr.flags = FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;

	if (REVERSE_Z)
	{
		createFsr.flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
	}
	createFsr.flags |= FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
	createFsr.fpMessage = nullptr;

	if (m_GlobalDebugCheckerMode != FSRDebugCheckerMode::Disabled)
	{
		createFsr.flags |= FFX_UPSCALE_ENABLE_DEBUG_CHECKING;
	}

	// Create the FSR context
	{
		ffx::ReturnCode retCode;
		retCode = ffx::CreateContext(m_UpscalingContext, nullptr, createFsr, backendDesc);
		//Couldn't create the ffx-api upscaling context
		assert(retCode == ffx::ReturnCode::Ok);
	}

	//Query created version
	ffxQueryGetProviderVersion getVersion = { 0 };
	getVersion.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;

	ffx::Query(m_UpscalingContext, getVersion);
	m_CurrentUpscaleContextVersionId = getVersion.versionId;
	m_CurrentUpscaleContextVersionName = getVersion.versionName;

	EngineLog::AddLog(L"Upscaler Context VersionID 0x%016llx, %S", m_CurrentUpscaleContextVersionId, m_CurrentUpscaleContextVersionName);

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

void FSR::SetGlobalDebugCheckerMode(FSRDebugCheckerMode mode, bool recreate)
{
}
