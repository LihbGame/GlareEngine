#include "FSR.h"
#include "Engine/EngineLog.h"
#include "Engine/EngineProfiling.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/CommandContext.h"
#include "Engine/GameCore.h"
#include "BufferManager.h"
#include "EngineGUI.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/CommandSignature.h"
#include "Graphics/Display.h"
#include "Engine/Scene.h"

using namespace GlareEngine::Display;

FSR* FSR::m_pFSRInstance = nullptr;

FSR::FSR()
{
}


void FSR::Execute(ComputeContext& Context, ColorBuffer& Input, ColorBuffer& Output)
{
	ScopedTimer SkyPassScope(L"FSR", Context);

	if (m_FrameInterpolationEnabled)
	{
		ffx::DispatchDescFrameGenerationSwapChainWaitForPresentsDX12 waitForPresentsDesc;
		ffx::ReturnCode errorCode = ffx::Dispatch(m_SwapChainContext, waitForPresentsDesc);
		GlareAssert(ASSERT_CRITICAL, errorCode == ffx::ReturnCode::Ok, L"OnPreFrame FrameGenerationSwapChain WaitForPresents failed: %d", (uint32_t)errorCode);
	}

	Camera* camera = EngineGlobal::gCurrentScene->GetCamera();
	if (m_UpscalingContext)
	{
		ffx::DispatchDescUpscale dispatchUpscale{};
		dispatchUpscale.commandList = Context.GetCommandList();

		dispatchUpscale.color = ffxApiGetResourceDX12(Input.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchUpscale.depth = ffxApiGetResourceDX12(g_SceneDepthBuffer.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchUpscale.motionVectors = ffxApiGetResourceDX12(g_GBuffer[Render::GBufferType::GBUFFER_MotionVector].GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchUpscale.exposure = ffxApiGetResourceDX12(nullptr, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchUpscale.output = ffxApiGetResourceDX12(Output.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);

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

		// Jitter is calculated earlier in the frame using a callback from the camera update
		dispatchUpscale.jitterOffset.x = -m_JitterX;
		dispatchUpscale.jitterOffset.y = -m_JitterY;
		dispatchUpscale.motionVectorScale.x = Display::g_RenderWidth;
		dispatchUpscale.motionVectorScale.y = Display::g_RenderHeight;
		dispatchUpscale.reset = m_ResetUpscale;
		dispatchUpscale.enableSharpening = m_SharpnessEnabled;
		dispatchUpscale.sharpness = m_Sharpness;

		dispatchUpscale.frameTimeDelta = static_cast<float>(EngineGlobal::gCurrentScene->GetDeltaTime() * 1000.f);

		dispatchUpscale.preExposure = 1.0f;
		dispatchUpscale.renderSize.width = Display::g_RenderWidth;
		dispatchUpscale.renderSize.height = Display::g_RenderHeight;
		dispatchUpscale.upscaleSize.width = Display::g_DisplayWidth;
		dispatchUpscale.upscaleSize.height = Display::g_DisplayHeight;

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

#ifdef DEBUG
		EngineGUI::AddRenderPassVisualizeTexture("FSR DEBUG", WStringToString(g_UnpackVelocityBuffer.GetName()), g_UnpackVelocityBuffer.GetHeight(), g_UnpackVelocityBuffer.GetWidth(), g_UnpackVelocityBuffer.GetSRV());
#endif
	}

	FfxApiResource backbuffer = ffxApiGetResourceDX12(Display::GetCurrentBuffer().GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);

	if (m_FrameInterpolationEnabled)
	{
		ffx::DispatchDescFrameGenerationPrepare dispatchFgPrep{};

		dispatchFgPrep.commandList = Context.GetCommandList();

		dispatchFgPrep.depth = ffxApiGetResourceDX12(g_SceneDepthBuffer.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchFgPrep.motionVectors = ffxApiGetResourceDX12(g_GBuffer[Render::GBufferType::GBUFFER_MotionVector].GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ, 0);
		dispatchFgPrep.flags = 0;

		dispatchFgPrep.jitterOffset.x = -m_JitterX;
		dispatchFgPrep.jitterOffset.y = -m_JitterY;
		dispatchFgPrep.motionVectorScale.x = Display::g_RenderWidth;
		dispatchFgPrep.motionVectorScale.y = Display::g_RenderHeight;

		// Cauldron keeps time in seconds, but FSR expects milliseconds
		dispatchFgPrep.frameTimeDelta = static_cast<float>(EngineGlobal::gCurrentScene->GetDeltaTime() * 1000.f);

		dispatchFgPrep.renderSize.width = Display::g_RenderWidth;
		dispatchFgPrep.renderSize.height = Display::g_RenderHeight;
		dispatchFgPrep.cameraFovAngleVertical = camera->GetFovY();

		if (REVERSE_Z)
		{
			dispatchFgPrep.cameraFar = camera->GetNearZ();
			dispatchFgPrep.cameraNear = camera->GetFarZ();
		}
		else
		{
			dispatchFgPrep.cameraFar = camera->GetFarZ();
			dispatchFgPrep.cameraNear = camera->GetNearZ();
		}
		dispatchFgPrep.viewSpaceToMetersFactor = 0.f;
		dispatchFgPrep.frameID = Display::GetFrameCount();

		// Update frame generation config
		//FfxApiResource hudLessResource =
			//SDKWrapper::ffxGetResourceApi(m_pHudLessTexture[m_curUiTextureIndex]->GetResource(), FFX_API_RESOURCE_STATE_COMPUTE_READ);

		m_FrameGenerationConfig.frameGenerationEnabled = m_FrameInterpolationEnabled;
		m_FrameGenerationConfig.flags = 0u;
		m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugTearLines ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0;
		m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugResetIndicators ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS : 0;
		m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugPacingLines ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES : 0;
		m_FrameGenerationConfig.flags |= m_DrawFrameGenerationDebugView ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW : 0;
		dispatchFgPrep.flags = m_FrameGenerationConfig.flags;  // TODO: maybe these should be distinct flags?
		m_FrameGenerationConfig.HUDLessColor = FfxApiResource({});
		m_FrameGenerationConfig.allowAsyncWorkloads = m_EnableAsyncCompute;
		// assume symmetric letterbox
		m_FrameGenerationConfig.generationRect.left = 0;
		m_FrameGenerationConfig.generationRect.top = 0;
		m_FrameGenerationConfig.generationRect.width = Display::g_DisplayWidth;
		m_FrameGenerationConfig.generationRect.height = Display::g_DisplayHeight;
		if (m_UseCallback)
		{
			m_FrameGenerationConfig.frameGenerationCallback = [](ffxDispatchDescFrameGeneration* params, void* pUserCtx) -> ffxReturnCode_t {
				return ffxDispatch(reinterpret_cast<ffxContext*>(pUserCtx), &params->header);
				};
			m_FrameGenerationConfig.frameGenerationCallbackUserContext = &m_FrameGenContext;
		}
		else
		{
			m_FrameGenerationConfig.frameGenerationCallback = nullptr;
			m_FrameGenerationConfig.frameGenerationCallbackUserContext = nullptr;
		}
		m_FrameGenerationConfig.onlyPresentGenerated = m_PresentInterpolatedOnly;
		m_FrameGenerationConfig.frameID = Display::GetFrameCount();

		void* ffxSwapChain;
		ffxSwapChain = g_SwapChain4.Get();

		m_FrameGenerationConfig.swapChain = ffxSwapChain;
		ffx::ReturnCode retCode = ffx::ReturnCode::ErrorParameter;

		retCode = ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);

		GlareEngine::GlareAssert(ASSERT_CRITICAL, !!retCode, L"Configuring FSR FG failed: %d", (uint32_t)retCode);

		ffx::DispatchDescFrameGenerationPrepareCameraInfo cameraConfig{};
		XMFLOAT3 cameraPosition = camera->GetPosition3f();
		XMFLOAT3 cameraUp = camera->GetUp3f();
		XMFLOAT3 cameraRight = camera->GetRight3f();
		XMFLOAT3 cameraLook = camera->GetLook3f();

		memcpy(cameraConfig.cameraPosition, &cameraPosition, 3 * sizeof(float));
		memcpy(cameraConfig.cameraUp, &cameraUp, 3 * sizeof(float));
		memcpy(cameraConfig.cameraRight, &cameraRight, 3 * sizeof(float));
		memcpy(cameraConfig.cameraForward, &cameraLook, 3 * sizeof(float));

		retCode = ffx::Dispatch(m_FrameGenContext, dispatchFgPrep, cameraConfig);

		GlareEngine::GlareAssert(ASSERT_CRITICAL, !!retCode, L"Dispatching FSR FG (upscaling data) failed: %d", (uint32_t)retCode);

		//FfxApiResource uiColor =
			//(m_uiRenderMode == 1) ? ffxApiGetResourceDX12(Display::g_UIBuffers.GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ)
			//: FfxApiResource({});
		FfxApiResource uiColor = FfxApiResource({});

		ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12 uiConfig{};
		uiConfig.uiResource = uiColor;
		uiConfig.flags = m_DoublebufferInSwapchain ? FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING : 0;
		ffx::Configure(m_SwapChainContext, uiConfig);
	}

	// Dispatch frame generation, if not using the callback
	if (m_FrameInterpolationEnabled && !m_UseCallback)
	{
		ffx::DispatchDescFrameGeneration dispatchFg{};

		dispatchFg.presentColor = backbuffer;
		dispatchFg.numGeneratedFrames = 1;

		// assume symmetric letterbox
		dispatchFg.generationRect.left = 0;
		dispatchFg.generationRect.top = 0;
		dispatchFg.generationRect.width = Display::g_DisplayWidth;
		dispatchFg.generationRect.height = Display::g_DisplayHeight;

		ffx::QueryDescFrameGenerationSwapChainInterpolationCommandListDX12 queryCmdList{};
		queryCmdList.pOutCommandList = &dispatchFg.commandList;
		ffx::Query(m_SwapChainContext, queryCmdList);

		ffx::QueryDescFrameGenerationSwapChainInterpolationTextureDX12 queryFiTexture{};
		queryFiTexture.pOutTexture = &dispatchFg.outputs[0];
		ffx::Query(m_SwapChainContext, queryFiTexture);

		dispatchFg.frameID = Display::GetFrameCount();
		dispatchFg.reset = m_ResetFrameInterpolation;

		ffx::ReturnCode retCode = ffx::Dispatch(m_FrameGenContext, dispatchFg);

		GlareAssert(ASSERT_CRITICAL, !!retCode, L"Dispatching Frame Generation failed: %d", (uint32_t)retCode);
	}

	m_ResetFrameInterpolation = false;
}

XMFLOAT2 FSR::GetFSRjitter()
{
	if (m_UpscalingContext)
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
		return XMFLOAT2(m_JitterX / Display::g_RenderWidth, m_JitterY / Display::g_RenderHeight);
	}
	else
		return XMFLOAT2(0, 0);
}

FSR::~FSR()
{
	UpdateUpscalingContext(false);

	if (m_SwapChainContext != nullptr)
	{
		// Restore the application's swapchain
		ffx::DestroyContext(m_SwapChainContext);
		m_SwapChainContext = nullptr;
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


ffxReturnCode_t UiCompositionCallback(ffxCallbackDescFrameGenerationPresent* params)
{
	EngineGlobal::gCurrentScene->DrawUI();
	return 0;
}

void FSR::UpdateUpscalingContext(bool enable)
{
	if (enable)
	{
		ffx::CreateBackendDX12Desc backendDesc{};
		backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
		backendDesc.device = g_Device;

		if (m_UpscalerEnabled)
		{
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

		//Create the FrameGen context
		if (m_FrameInterpolationEnabled)
		{
			ffx::CreateContextDescFrameGeneration createFg{};
			createFg.displaySize = { Display::g_DisplayWidth,Display::g_DisplayHeight };
			createFg.maxRenderSize = { Display::g_DisplayWidth, Display::g_DisplayHeight };
			if (REVERSE_Z)
				createFg.flags |= FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED;
			createFg.flags |= FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE;

			if (m_EnableAsyncCompute)
			{
				createFg.flags |= FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT;
			}
			if (m_GlobalDebugCheckerMode != FSRDebugCheckerMode::Disabled)
			{
				createFg.flags |= FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING;
			}


			createFg.backBufferFormat = GetFfxSurfaceFormat(Display::g_SwapChainFormat);
			ffx::ReturnCode retCode;
			retCode = ffx::CreateContext(m_FrameGenContext, nullptr, createFg, backendDesc);
			//Couldn't create the ffxapi framegen context
			assert(retCode == ffx::ReturnCode::Ok);

			void* ffxSwapChain;
			ffxSwapChain = g_SwapChain4.Get();

			// Configure frame generation
			//FfxApiResource hudLessResource = ffxGetResourceApi(Display::GetCurrentSceneColorBufferBuffer().GetResource(), FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);

			m_FrameGenerationConfig.frameGenerationEnabled = false;
			m_FrameGenerationConfig.frameGenerationCallback = [](ffxDispatchDescFrameGeneration* params, void* pUserCtx) -> ffxReturnCode_t
				{
					return ffxDispatch(reinterpret_cast<ffxContext*>(pUserCtx), &params->header);
				};
			m_FrameGenerationConfig.frameGenerationCallbackUserContext = &m_FrameGenContext;
			if (m_uiRenderMode == 2)
			{
				m_FrameGenerationConfig.presentCallback = [](ffxCallbackDescFrameGenerationPresent* params, void* self) -> auto { 
					return UiCompositionCallback(params); };
				m_FrameGenerationConfig.presentCallbackUserContext = this;
			}
			else
			{
				m_FrameGenerationConfig.presentCallback = nullptr;
				m_FrameGenerationConfig.presentCallbackUserContext = nullptr;
			}
			m_FrameGenerationConfig.swapChain = ffxSwapChain;
			m_FrameGenerationConfig.HUDLessColor = FfxApiResource({});

			m_FrameGenerationConfig.frameID = Display::GetFrameCount();

			retCode = ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);
			GlareAssert(ASSERT_CRITICAL, retCode == ffx::ReturnCode::Ok, L"Couldn't create the ffxapi upscaling context: %d", (uint32_t)retCode);

			FfxApiEffectMemoryUsage gpuMemoryUsageFrameGeneration;
			ffx::QueryDescFrameGenerationGetGPUMemoryUsage frameGenGetGPUMemoryUsage{};
			frameGenGetGPUMemoryUsage.gpuMemoryUsageFrameGeneration = &gpuMemoryUsageFrameGeneration;
			ffx::Query(m_FrameGenContext, frameGenGetGPUMemoryUsage);

			EngineLog::AddLog(L"FrameGeneration Context VRAM totalUsageInBytes %f MB aliasableUsageInBytes %f MB", gpuMemoryUsageFrameGeneration.totalUsageInBytes / 1048576.f, gpuMemoryUsageFrameGeneration.aliasableUsageInBytes / 1048576.f);

			FfxApiEffectMemoryUsage gpuMemoryUsageFrameGenerationSwapchain;
			ffx::QueryFrameGenerationSwapChainGetGPUMemoryUsageDX12 frameGenSwapchainGetGPUMemoryUsage{};
			frameGenSwapchainGetGPUMemoryUsage.gpuMemoryUsageFrameGenerationSwapchain = &gpuMemoryUsageFrameGenerationSwapchain;
			ffx::Query(m_SwapChainContext, frameGenSwapchainGetGPUMemoryUsage);

			EngineLog::AddLog(L"Swap chain Context VRAM totalUsageInBytes %f MB aliasableUsageInBytes %f MB", gpuMemoryUsageFrameGenerationSwapchain.totalUsageInBytes / 1048576.f, gpuMemoryUsageFrameGenerationSwapchain.aliasableUsageInBytes / 1048576.f);
		}

	}
	else
	{
		if (m_FrameInterpolationEnabled && m_FrameGenContext)
		{
			void* ffxSwapChain;
			ffxSwapChain = Display::g_SwapChain4.Get();

			// disable frame generation before destroying context
			// also unset present callback, HUDLessColor and UiTexture to have the swapchain only present the backbuffer
			m_FrameGenerationConfig.frameGenerationEnabled = false;
			m_FrameGenerationConfig.swapChain = ffxSwapChain;
			m_FrameGenerationConfig.presentCallback = nullptr;
			m_FrameGenerationConfig.HUDLessColor = FfxApiResource({});
			ffx::Configure(m_FrameGenContext, m_FrameGenerationConfig);

			ffx::ConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12 uiConfig{};
			uiConfig.uiResource = {};
			uiConfig.flags = 0;
			ffx::Configure(m_SwapChainContext, uiConfig);

			ffx::DestroyContext(m_FrameGenContext);
			m_FrameGenContext = nullptr;
		}

		if (m_UpscalingContext)
		{
			ffx::DestroyContext(m_UpscalingContext);
			m_UpscalingContext = nullptr;
		}
	}
}

void FSR::CreateSwapChain(ffx::CreateContextDescFrameGenerationSwapChainForHwndDX12& createSwapChainDesc)
{
	ffx::ReturnCode retCode = ffx::CreateContext(m_SwapChainContext, nullptr, createSwapChainDesc);
	assert(retCode == ffx::ReturnCode::Ok);
	createSwapChainDesc.dxgiFactory->Release();
}

void FSR::DrawUI()
{
	if (Render::GetAntiAliasingType() == Render::AntiAliasingType::FSR)
	{
		if (ImGui::CollapsingHeader("FSR", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Combo("FSR Quality", &m_NewPreset, "NativeAA\0Quality\0Balanced\0Performance\0UltraPerformance");
			if (m_NewPreset != (int32_t)m_ScalePreset)
			{
				UpdateUpscalerPreset(m_NewPreset);
			}
			ImGui::SliderVerticalFloat("Sharpness:", &m_Sharpness, 0.0f, 1.0f);
			ImGui::Checkbox("Upscaler Debug View", &m_DrawUpscalerDebugView);
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

	Display::g_bFSRUpscale = true;
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
