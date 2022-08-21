#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
//#include "TemporalEffects.h"
namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		DepthBuffer g_SceneDepthBuffer;
		ColorBuffer g_SceneColorBuffer;

		//MSAA Buffer
		DepthBuffer g_SceneMSAADepthBuffer;
		ColorBuffer g_SceneMSAAColorBuffer;

		//ColorBuffer g_PostEffectsBuffer;
		//ColorBuffer g_VelocityBuffer;
		//ColorBuffer g_OverlayBuffer;
		//ColorBuffer g_HorizontalBuffer;

		//ColorBuffer g_SSAOFullScreen(Color(1.0f, 1.0f, 1.0f));
		//ColorBuffer g_LinearDepth[2];
		//ColorBuffer g_MinMaxDepth8;
		//ColorBuffer g_MinMaxDepth16;
		//ColorBuffer g_MinMaxDepth32;
		//ColorBuffer g_DepthDownsize1;
		//ColorBuffer g_DepthDownsize2;
		//ColorBuffer g_DepthDownsize3;
		//ColorBuffer g_DepthDownsize4;
		//ColorBuffer g_DepthTiled1;
		//ColorBuffer g_DepthTiled2;
		//ColorBuffer g_DepthTiled3;
		//ColorBuffer g_DepthTiled4;
		//ColorBuffer g_AOMerged1;
		//ColorBuffer g_AOMerged2;
		//ColorBuffer g_AOMerged3;
		//ColorBuffer g_AOMerged4;
		//ColorBuffer g_AOSmooth1;
		//ColorBuffer g_AOSmooth2;
		//ColorBuffer g_AOSmooth3;
		//ColorBuffer g_AOHighQuality1;
		//ColorBuffer g_AOHighQuality2;
		//ColorBuffer g_AOHighQuality3;
		//ColorBuffer g_AOHighQuality4;

		//ColorBuffer g_DoFTileClass[2];
		//ColorBuffer g_DoFPresortBuffer;
		//ColorBuffer g_DoFPrefilter;
		//ColorBuffer g_DoFBlurColor[2];
		//ColorBuffer g_DoFBlurAlpha[2];
		//StructuredBuffer g_DoFWorkQueue;
		//StructuredBuffer g_DoFFastQueue;
		//StructuredBuffer g_DoFFixupQueue;

		//ColorBuffer g_MotionPrepBuffer;
		//ColorBuffer g_LumaBuffer;
		//ColorBuffer g_TemporalColor[2];
		//ColorBuffer g_aBloomUAV1[2];    // 640x384 (1/3)
		//ColorBuffer g_aBloomUAV2[2];    // 320x192 (1/6)  
		//ColorBuffer g_aBloomUAV3[2];    // 160x96  (1/12)
		//ColorBuffer g_aBloomUAV4[2];    // 80x48   (1/24)
		//ColorBuffer g_aBloomUAV5[2];    // 40x24   (1/48)
		//ColorBuffer g_LumaLR;
		//ByteAddressBuffer g_Histogram;
		//ByteAddressBuffer g_FXAAWorkCounters;
		//ByteAddressBuffer g_FXAAWorkQueue;
		//TypedBuffer g_FXAAColorQueue(DXGI_FORMAT_R11G11B10_FLOAT);

		//// For testing GenerateMipMaps()
		//ColorBuffer g_GenMipsBuffer;

		DXGI_FORMAT DefaultHDRColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
	}
}

#define T2X_COLOR_FORMAT DXGI_FORMAT_R10G10B10A2_UNORM
#define HDR_MOTION_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DSV_FORMAT DXGI_FORMAT_D32_FLOAT



void GlareEngine::DirectX12Graphics::InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
	GraphicsContext& InitContext = GraphicsContext::Begin();

	//const uint32_t bufferWidth1 = (NativeWidth + 1) / 2;
	//const uint32_t bufferWidth2 = (NativeWidth + 3) / 4;
	//const uint32_t bufferWidth3 = (NativeWidth + 7) / 8;
	//const uint32_t bufferWidth4 = (NativeWidth + 15) / 16;
	//const uint32_t bufferWidth5 = (NativeWidth + 31) / 32;
	//const uint32_t bufferWidth6 = (NativeWidth + 63) / 64;
	//const uint32_t bufferHeight1 = (NativeHeight + 1) / 2;
	//const uint32_t bufferHeight2 = (NativeHeight + 3) / 4;
	//const uint32_t bufferHeight3 = (NativeHeight + 7) / 8;
	//const uint32_t bufferHeight4 = (NativeHeight + 15) / 16;
	//const uint32_t bufferHeight5 = (NativeHeight + 31) / 32;
	//const uint32_t bufferHeight6 = (NativeHeight + 63) / 64;


	g_SceneColorBuffer.Create(L"Main Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", NativeWidth, NativeHeight, DSV_FORMAT, REVERSE_Z);
	
	//MSAA
	//g_SceneMSAADepthBuffer
	g_SceneMSAAColorBuffer.SetMsaaMode(MSAACOUNT, MSAACOUNT);
	g_SceneMSAAColorBuffer.Create(L"Main MSAA Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneMSAADepthBuffer.Create(L"Scene MSAA Depth Buffer", NativeWidth, NativeHeight, MSAACOUNT, DSV_FORMAT, REVERSE_Z);

	//g_VelocityBuffer.Create(L"Motion Vectors", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R32_UINT);
	//g_PostEffectsBuffer.Create(L"Post Effects Buffer", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R32_UINT);

	//g_LinearDepth[0].Create(L"Linear Depth 0", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_UNORM);
	//g_LinearDepth[1].Create(L"Linear Depth 1", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_UNORM);
	//g_MinMaxDepth8.Create(L"MinMaxDepth 8x8", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_UINT, esram);
	//g_MinMaxDepth16.Create(L"MinMaxDepth 16x16", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_UINT, esram);
	//g_MinMaxDepth32.Create(L"MinMaxDepth 32x32", bufferWidth5, bufferHeight5, 1, DXGI_FORMAT_R32_UINT, esram);

	

	//g_SSAOFullScreen.Create(L"SSAO Full Res", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM);

	
	//g_DepthDownsize1.Create(L"Depth Down-Sized 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R32_FLOAT, esram);
	//g_DepthDownsize2.Create(L"Depth Down-Sized 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R32_FLOAT, esram);
	//g_DepthDownsize3.Create(L"Depth Down-Sized 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_FLOAT, esram);
	//g_DepthDownsize4.Create(L"Depth Down-Sized 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_FLOAT, esram);
	//g_DepthTiled1.CreateArray(L"Depth De-Interleaved 1", bufferWidth3, bufferHeight3, 16, DXGI_FORMAT_R16_FLOAT, esram);
	//g_DepthTiled2.CreateArray(L"Depth De-Interleaved 2", bufferWidth4, bufferHeight4, 16, DXGI_FORMAT_R16_FLOAT, esram);
	//g_DepthTiled3.CreateArray(L"Depth De-Interleaved 3", bufferWidth5, bufferHeight5, 16, DXGI_FORMAT_R16_FLOAT, esram);
	//g_DepthTiled4.CreateArray(L"Depth De-Interleaved 4", bufferWidth6, bufferHeight6, 16, DXGI_FORMAT_R16_FLOAT, esram);
	//g_AOMerged1.Create(L"AO Re-Interleaved 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOMerged2.Create(L"AO Re-Interleaved 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOMerged3.Create(L"AO Re-Interleaved 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOMerged4.Create(L"AO Re-Interleaved 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOSmooth1.Create(L"AO Smoothed 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOSmooth2.Create(L"AO Smoothed 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOSmooth3.Create(L"AO Smoothed 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOHighQuality1.Create(L"AO High Quality 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOHighQuality2.Create(L"AO High Quality 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOHighQuality3.Create(L"AO High Quality 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_AOHighQuality4.Create(L"AO High Quality 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM, esram);

	//g_DoFTileClass[0].Create(L"DoF Tile Classification Buffer 0", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R11G11B10_FLOAT);
	//g_DoFTileClass[1].Create(L"DoF Tile Classification Buffer 1", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R11G11B10_FLOAT);
	//
	//g_DoFPresortBuffer.Create(L"DoF Presort Buffer", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
	//g_DoFPrefilter.Create(L"DoF PreFilter Buffer", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
	//g_DoFBlurColor[0].Create(L"DoF Blur Color", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
	//g_DoFBlurColor[1].Create(L"DoF Blur Color", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
	//g_DoFBlurAlpha[0].Create(L"DoF FG Alpha", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_DoFBlurAlpha[1].Create(L"DoF FG Alpha", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_DoFWorkQueue.Create(L"DoF Work Queue", bufferWidth4 * bufferHeight4, 4, esram);
	//g_DoFFastQueue.Create(L"DoF Fast Queue", bufferWidth4 * bufferHeight4, 4, esram);
	//g_DoFFixupQueue.Create(L"DoF Fixup Queue", bufferWidth4 * bufferHeight4, 4, esram);


	//g_TemporalColor[0].Create(L"Temporal Color 0", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	//g_TemporalColor[1].Create(L"Temporal Color 1", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	//TemporalEffects::ClearHistory(InitContext);

	
	//g_MotionPrepBuffer.Create(L"Motion Blur Prep", bufferWidth1, bufferHeight1, 1, HDR_MOTION_FORMAT, esram);
	

	// This is useful for storing per-pixel weights such as motion strength or pixel luminance
	//g_LumaBuffer.Create(L"Luminance", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM, esram);
	//g_Histogram.Create(L"Histogram", 256, 4, esram);

	// Divisible by 128 so that after dividing by 16, we still have multiples of 8x8 tiles.  The bloom
	// dimensions must be at least 1/4 native resolution to avoid undersampling.
	//uint32_t kBloomWidth = bufferWidth > 2560 ? Math::AlignUp(bufferWidth / 4, 128) : 640;
	//uint32_t kBloomHeight = bufferHeight > 1440 ? Math::AlignUp(bufferHeight / 4, 128) : 384;
	//uint32_t kBloomWidth = bufferWidth > 2560 ? 1280 : 640;
	//uint32_t kBloomHeight = bufferHeight > 1440 ? 768 : 384;

	
	//g_LumaLR.Create(L"Luma Buffer", kBloomWidth, kBloomHeight, 1, DXGI_FORMAT_R8_UINT, esram);
	//g_aBloomUAV1[0].Create(L"Bloom Buffer 1a", kBloomWidth, kBloomHeight, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV1[1].Create(L"Bloom Buffer 1b", kBloomWidth, kBloomHeight, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV2[0].Create(L"Bloom Buffer 2a", kBloomWidth / 2, kBloomHeight / 2, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV2[1].Create(L"Bloom Buffer 2b", kBloomWidth / 2, kBloomHeight / 2, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV3[0].Create(L"Bloom Buffer 3a", kBloomWidth / 4, kBloomHeight / 4, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV3[1].Create(L"Bloom Buffer 3b", kBloomWidth / 4, kBloomHeight / 4, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV4[0].Create(L"Bloom Buffer 4a", kBloomWidth / 8, kBloomHeight / 8, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV4[1].Create(L"Bloom Buffer 4b", kBloomWidth / 8, kBloomHeight / 8, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV5[0].Create(L"Bloom Buffer 5a", kBloomWidth / 16, kBloomHeight / 16, 1, DefaultHdrColorFormat, esram);
	//g_aBloomUAV5[1].Create(L"Bloom Buffer 5b", kBloomWidth / 16, kBloomHeight / 16, 1, DefaultHdrColorFormat, esram);
	

	//const uint32_t kFXAAWorkSize = bufferWidth * bufferHeight / 4 + 128;
	//g_FXAAWorkQueue.Create(L"FXAA Work Queue", kFXAAWorkSize, sizeof(uint32_t), esram);
	//g_FXAAColorQueue.Create(L"FXAA Color Queue", kFXAAWorkSize, sizeof(uint32_t), esram);
	//g_FXAAWorkCounters.Create(L"FXAA Work Counters", 2, sizeof(uint32_t));
	//InitContext.ClearUAV(g_FXAAWorkCounters);
	
	//g_GenMipsBuffer.Create(L"GenMips", bufferWidth, bufferHeight, 0, DXGI_FORMAT_R11G11B10_FLOAT, esram);

	//g_OverlayBuffer.Create(L"UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, esram);
	//g_HorizontalBuffer.Create(L"Bicubic Intermediate", g_DisplayWidth, bufferHeight, 1, DefaultHdrColorFormat, esram);


	InitContext.Finish();
}

void GlareEngine::DirectX12Graphics::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
	//resize display buffer
	g_SceneColorBuffer.Create(L"Main Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", NativeWidth, NativeHeight, DSV_FORMAT, REVERSE_Z);
	//resize  MSAA buffer
	g_SceneMSAAColorBuffer.Create(L"Main MSAA Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneMSAADepthBuffer.Create(L"Scene MSAA Depth Buffer", NativeWidth, NativeHeight, MSAACOUNT, DSV_FORMAT, REVERSE_Z);
}

void GlareEngine::DirectX12Graphics::DestroyRenderingBuffers()
{
	g_SceneDepthBuffer.Destroy();
	g_SceneColorBuffer.Destroy();
	/*g_VelocityBuffer.Destroy();
	g_OverlayBuffer.Destroy();
	g_HorizontalBuffer.Destroy();
	g_PostEffectsBuffer.Destroy();*/

	//g_ShadowBuffer.Destroy();

	//g_SSAOFullScreen.Destroy();
	//g_LinearDepth[0].Destroy();
	//g_LinearDepth[1].Destroy();
	//g_MinMaxDepth8.Destroy();
	//g_MinMaxDepth16.Destroy();
	//g_MinMaxDepth32.Destroy();
	//g_DepthDownsize1.Destroy();
	//g_DepthDownsize2.Destroy();
	//g_DepthDownsize3.Destroy();
	//g_DepthDownsize4.Destroy();
	//g_DepthTiled1.Destroy();
	//g_DepthTiled2.Destroy();
	//g_DepthTiled3.Destroy();
	//g_DepthTiled4.Destroy();
	//g_AOMerged1.Destroy();
	//g_AOMerged2.Destroy();
	//g_AOMerged3.Destroy();
	//g_AOMerged4.Destroy();
	//g_AOSmooth1.Destroy();
	//g_AOSmooth2.Destroy();
	//g_AOSmooth3.Destroy();
	//g_AOHighQuality1.Destroy();
	//g_AOHighQuality2.Destroy();
	//g_AOHighQuality3.Destroy();
	//g_AOHighQuality4.Destroy();

	//g_DoFTileClass[0].Destroy();
	//g_DoFTileClass[1].Destroy();
	//g_DoFPresortBuffer.Destroy();
	//g_DoFPrefilter.Destroy();
	//g_DoFBlurColor[0].Destroy();
	//g_DoFBlurColor[1].Destroy();
	//g_DoFBlurAlpha[0].Destroy();
	//g_DoFBlurAlpha[1].Destroy();
	//g_DoFWorkQueue.Destroy();
	//g_DoFFastQueue.Destroy();
	//g_DoFFixupQueue.Destroy();

	//g_MotionPrepBuffer.Destroy();
	//g_LumaBuffer.Destroy();
	//g_TemporalColor[0].Destroy();
	//g_TemporalColor[1].Destroy();
	//g_aBloomUAV1[0].Destroy();
	//g_aBloomUAV1[1].Destroy();
	//g_aBloomUAV2[0].Destroy();
	//g_aBloomUAV2[1].Destroy();
	//g_aBloomUAV3[0].Destroy();
	//g_aBloomUAV3[1].Destroy();
	//g_aBloomUAV4[0].Destroy();
	//g_aBloomUAV4[1].Destroy();
	//g_aBloomUAV5[0].Destroy();
	//g_aBloomUAV5[1].Destroy();
	//g_LumaLR.Destroy();
	//g_Histogram.Destroy();
	//g_FXAAWorkCounters.Destroy();
	//g_FXAAWorkQueue.Destroy();
	//g_FXAAColorQueue.Destroy();

	//g_GenMipsBuffer.Destroy();
}
