#include "FSR.h"
#include "Engine/EngineLog.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"
#include "imgui.h"
#include "Engine/GameCore.h"
#include "BufferManager.h"
#include "EngineGUI.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/CommandSignature.h"
#include "Graphics/Display.h"
#include "EngineGUI.h"
#include "Engine/Scene.h"

#include "ffx-api/dx12/ffx_api_dx12.hpp"
#include "PostProcessing/PostProcessing.h"

FSR* FSR::m_pFSRInstance = nullptr;

void FSR::Execute(ID3D12GraphicsCommandList* pCmdList)
{
	ffx::DispatchDescUpscale dispatchUpscale{};
	dispatchUpscale.commandList = pCmdList;

	dispatchUpscale.color = ffxApiGetResourceDX12(ScreenProcessing::GetLastPostprocessRT()-> GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
	dispatchUpscale.depth = ffxApiGetResourceDX12(g_SceneDepthBuffer.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
	dispatchUpscale.motionVectors = ffxApiGetResourceDX12(g_VelocityBuffer.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
	dispatchUpscale.exposure = ffxApiGetResourceDX12(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
	dispatchUpscale.output = ffxApiGetResourceDX12(ScreenProcessing::GetCurrentPostprocessRT()->GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);

	if (m_UseMask)
	{
		dispatchUpscale.reactive = ffxApiGetResourceDX12(g_ReactiveMaskBuffer.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchUpscale.transparencyAndComposition = ffxApiGetResourceDX12(g_TransparentMaskBuffer.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
	}
	else
	{
		dispatchUpscale.reactive = ffxApiGetResourceDX12(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchUpscale.transparencyAndComposition = ffxApiGetResourceDX12(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
	}

	dispatchUpscale.frameTimeDelta = static_cast<float>(EngineGlobal::gCurrentScene->GetDeltaTime() * 1000.f);

	dispatchUpscale.preExposure = 1.0f;
	dispatchUpscale.renderSize.width = Display::g_RenderWidth;
	dispatchUpscale.renderSize.height = Display::g_RenderHeight;
	dispatchUpscale.upscaleSize.width = Display::g_DisplayWidth;
	dispatchUpscale.upscaleSize.height = Display::g_DisplayHeight;

	Camera* camera=EngineGlobal::gCurrentScene->GetCamera();
	// Setup camera params as required
	dispatchUpscale.cameraFovAngleVertical = camera->GetFovY();

	if (REVERSE_Z)
	{
		dispatchUpscale.cameraFar = camera->GetNearZ();
		dispatchUpscale.cameraNear = camera->GetFarZ();
	}
	else
	{
		dispatchUpscale.cameraFar = camera->GetFarZ();
		dispatchUpscale.cameraNear = camera->GetNearZ();
	}

	dispatchUpscale.flags = 0;
	dispatchUpscale.flags |= m_DrawUpscalerDebugView ? FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW : 0;

	ffx::ReturnCode retCode = ffx::Dispatch(m_UpscalingContext, dispatchUpscale);

	//check Dispatching FSR upscaling failed
	assert(!!retCode);

	ColorBuffer* LastPostprocessRT = ScreenProcessing::GetLastPostprocessRT();
	ColorBuffer* CurrentPostprocessRT = ScreenProcessing::GetCurrentPostprocessRT();
	std::swap(LastPostprocessRT, CurrentPostprocessRT);
}

XMFLOAT2 FSR::GetFSRjitter()
{
	// Increment jitter index for frame
	++m_JitterIndex;

	// Update FSR jitter for built in TAA
	ffx::ReturnCode                     retCode;
	int32_t                             jitterPhaseCount;
	ffx::QueryDescUpscaleGetJitterPhaseCount getJitterPhaseDesc{};
	getJitterPhaseDesc.displayWidth = Display::g_DisplayWidth;
	getJitterPhaseDesc.renderWidth = Display::g_RenderWidth;
	getJitterPhaseDesc.pOutPhaseCount = &jitterPhaseCount;

	retCode = ffx::Query(m_UpscalingContext, getJitterPhaseDesc);
	assert(retCode == ffx::ReturnCode::Ok);

	ffx::QueryDescUpscaleGetJitterOffset getJitterOffsetDesc{};
	getJitterOffsetDesc.index = m_JitterIndex;
	getJitterOffsetDesc.phaseCount = jitterPhaseCount;
	getJitterOffsetDesc.pOutX = &m_JitterX;
	getJitterOffsetDesc.pOutY = &m_JitterY;

	retCode = ffx::Query(m_UpscalingContext, getJitterOffsetDesc);

	assert(retCode == ffx::ReturnCode::Ok);

	return XMFLOAT2(-2.f * m_JitterX / Display::g_RenderWidth, 2.f * m_JitterY / Display::g_RenderHeight);
}

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

void FSR::UpdateUpscalingContext(bool enable)
{
	if (enable)
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

		//Debug message for FSR
		SetGlobalDebugCheckerMode(m_GlobalDebugCheckerMode);
	}
	else
	{
		if (m_UpscalingContext)
		{
			ffx::DestroyContext(m_UpscalingContext);
			m_UpscalingContext = nullptr;
		}
	}
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

void FSR::SetGlobalDebugCheckerMode(FSRDebugCheckerMode mode)
{
	if (m_UpscalingContext)
	{
		ffx::ConfigureDescGlobalDebug1 m_GlobalDebugConfig{};
		m_GlobalDebugConfig.debugLevel = 0; //not implemented. Value doesn't matter.
		if (mode == FSRDebugCheckerMode::Disabled || mode == FSRDebugCheckerMode::EnabledNoMessageCallback)
		{
			m_GlobalDebugConfig.fpMessage = nullptr;
		}
		else if (mode == FSRDebugCheckerMode::EnabledWithMessageCallback)
		{
			m_GlobalDebugConfig.fpMessage = &FSR::FSRMsgCallback;
		}

		ffx::ReturnCode retCode = ffx::Configure(m_UpscalingContext, m_GlobalDebugConfig);
		//Couldn't configure global debug config on the ffx api upscaling context
		assert(retCode == ffx::ReturnCode::Ok);
	}
}
