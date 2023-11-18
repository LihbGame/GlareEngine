#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "Render.h"
#include "PostProcessing/TemporalAA.h"



namespace GlareEngine
{
	DepthBuffer				g_SceneDepthBuffer;

	ColorBuffer				g_SceneColorBuffer;

	ColorBuffer				g_PostEffectsBuffer;

	//ping-pong with main scene RT in post process chain
	ColorBuffer				g_PostColorBuffer;

	//MSAA Buffer
	DepthBuffer				g_SceneMSAADepthBuffer;

	ColorBuffer				g_SceneMSAAColorBuffer;

	//Bloom
	ColorBuffer				g_aBloomUAV1[2];

	ColorBuffer				g_aBloomUAV2[2];

	ColorBuffer				g_aBloomUAV3[2];

	ColorBuffer				g_aBloomUAV4[2];

	ColorBuffer				g_aBloomUAV5[2];

	ColorBuffer				g_LumaBloom;

	//HDR
	ByteAddressBuffer		g_Histogram;

	ColorBuffer				g_LumaBuffer;
	
	ColorBuffer				g_LinearDepth;

	ColorBuffer				g_SSAOFullScreen(Color(1.0f, 1.0f, 1.0f));				//Clear Color(1.0f, 1.0f, 1.0f)
	//R8 Format
	ColorBuffer				g_BlurTemp_HalfBuffer_R8(Color(1.0f, 1.0f, 1.0f));		//Clear Color(1.0f, 1.0f, 1.0f)

	ColorBuffer				g_VelocityBuffer;

	ColorBuffer				g_MotionPrepBuffer;

	ColorBuffer				g_TemporalColor[2];

	//ColorBuffer g_OverlayBuffer;
	//ColorBuffer g_HorizontalBuffer;

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

	//ByteAddressBuffer g_FXAAWorkCounters;
	//ByteAddressBuffer g_FXAAWorkQueue;
	//TypedBuffer g_FXAAColorQueue(DXGI_FORMAT_R11G11B10_FLOAT);

	//// For testing GenerateMipMaps()
	//ColorBuffer g_GenMipsBuffer;

	//DXGI_FORMAT_R11G11B10_FLOAT
	DXGI_FORMAT DefaultHDRColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
}


void GlareEngine::InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
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
	g_PostEffectsBuffer.Create(L"Post Effects Buffer", NativeWidth, NativeWidth, 1, DXGI_FORMAT_R32_UINT);

	//MSAA buffer
	//g_SceneMSAADepthBuffer
	g_SceneMSAAColorBuffer.SetMsaaMode(MSAACOUNT, MSAACOUNT);
	g_SceneMSAAColorBuffer.Create(L"Main MSAA Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneMSAADepthBuffer.Create(L"Scene MSAA Depth Buffer", NativeWidth, NativeHeight, MSAACOUNT, DSV_FORMAT, REVERSE_Z);

	//Bloom buffer
	//No need for a full-screen bloom, but the length and width are multiples of 8 and the number of cs scheduling threads match
	//Divisible by 128 so that after dividing by 16, we still have multiples of 8x8 tiles. Look at the CS shader to understand why it is 128
	uint32_t BloomWidth = Math::AlignUp(NativeWidth / 2, 128);
	uint32_t BloomHeight = Math::AlignUp(NativeHeight / 2, 128);
	g_LumaBloom.Create(L"Luminance Buffer", BloomWidth, BloomHeight, 1, DXGI_FORMAT_R8_UINT);
	// 1/2 
	g_aBloomUAV1[0].Create(L"Bloom Buffer 1a", BloomWidth, BloomHeight, 1, DefaultHDRColorFormat);
	g_aBloomUAV1[1].Create(L"Bloom Buffer 1b", BloomWidth, BloomHeight, 1, DefaultHDRColorFormat);
	// 1/4
	g_aBloomUAV2[0].Create(L"Bloom Buffer 2a", BloomWidth / 2, BloomHeight / 2, 1, DefaultHDRColorFormat);
	g_aBloomUAV2[1].Create(L"Bloom Buffer 2b", BloomWidth / 2, BloomHeight / 2, 1, DefaultHDRColorFormat);
	// 1/8
	g_aBloomUAV3[0].Create(L"Bloom Buffer 3a", BloomWidth / 4, BloomHeight / 4, 1, DefaultHDRColorFormat);
	g_aBloomUAV3[1].Create(L"Bloom Buffer 3b", BloomWidth / 4, BloomHeight / 4, 1, DefaultHDRColorFormat);
	// 1/16
	g_aBloomUAV4[0].Create(L"Bloom Buffer 4a", BloomWidth / 8, BloomHeight / 8, 1, DefaultHDRColorFormat);
	g_aBloomUAV4[1].Create(L"Bloom Buffer 4b", BloomWidth / 8, BloomHeight / 8, 1, DefaultHDRColorFormat);
	// 1/32
	g_aBloomUAV5[0].Create(L"Bloom Buffer 5a", BloomWidth / 16, BloomHeight / 16, 1, DefaultHDRColorFormat);
	g_aBloomUAV5[1].Create(L"Bloom Buffer 5b", BloomWidth / 16, BloomHeight / 16, 1, DefaultHDRColorFormat);

	//Luminance Histogram
	g_Histogram.Create(L"Histogram", 256, 4);
	g_LumaBuffer.Create(L"Luminance", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R8_UNORM);

	//Linear Depth
	g_LinearDepth.Create(L"Linear Depth", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16_UNORM);

	g_SSAOFullScreen.Create(L"SSAO Full Resolution", NativeWidth / 2, NativeHeight / 2, 1, DXGI_FORMAT_R8_UNORM);

	g_PostColorBuffer.Create(L"Post Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);

	g_BlurTemp_HalfBuffer_R8.Create(L"Blur Half Temporary Buffer(R8_UNORM)", NativeWidth / 2, NativeHeight / 2, 1, DXGI_FORMAT_R8_UNORM);

	g_VelocityBuffer.Create(L"Motion Vectors", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R32_UINT);

	g_MotionPrepBuffer.Create(L"Motion Blur Prep", NativeWidth / 2, NativeHeight / 2, 1, HDR_MOTION_FORMAT);

	g_TemporalColor[0].Create(L"Temporal Color 0", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	g_TemporalColor[1].Create(L"Temporal Color 1", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	TemporalAA::ClearHistory(InitContext);

	//g_MinMaxDepth8.Create(L"MinMaxDepth 8x8", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_UINT, esram);
	//g_MinMaxDepth16.Create(L"MinMaxDepth 16x16", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_UINT, esram);
	//g_MinMaxDepth32.Create(L"MinMaxDepth 32x32", bufferWidth5, bufferHeight5, 1, DXGI_FORMAT_R32_UINT, esram);

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

	
	//g_MotionPrepBuffer.Create(L"Motion Blur Prep", bufferWidth1, bufferHeight1, 1, HDR_MOTION_FORMAT, esram);
	

	//const uint32_t kFXAAWorkSize = bufferWidth * bufferHeight / 4 + 128;
	//g_FXAAWorkQueue.Create(L"FXAA Work Queue", kFXAAWorkSize, sizeof(uint32_t), esram);
	//g_FXAAColorQueue.Create(L"FXAA Color Queue", kFXAAWorkSize, sizeof(uint32_t), esram);
	//g_FXAAWorkCounters.Create(L"FXAA Work Counters", 2, sizeof(uint32_t));
	//InitContext.ClearUAV(g_FXAAWorkCounters);
	
	//g_GenMipsBuffer.Create(L"GenMips", bufferWidth, bufferHeight, 0, DXGI_FORMAT_R11G11B10_FLOAT, esram);

	//g_OverlayBuffer.Create(L"UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM, esram);
	//g_HorizontalBuffer.Create(L"Bicubic Intermediate", g_DisplayWidth, bufferHeight, 1, DefaultHdrColorFormat, esram);


	InitContext.Finish();
}

void GlareEngine::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
	//resize display buffer
	g_SceneColorBuffer.Create(L"Main Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", NativeWidth, NativeHeight, DSV_FORMAT, REVERSE_Z);
	g_PostEffectsBuffer.Create(L"Post Effects Buffer", NativeWidth, NativeWidth, 1, DXGI_FORMAT_R32_UINT);

	//resize  MSAA buffer
	g_SceneMSAAColorBuffer.Create(L"Main MSAA Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);
	g_SceneMSAADepthBuffer.Create(L"Scene MSAA Depth Buffer", NativeWidth, NativeHeight, MSAACOUNT, DSV_FORMAT, REVERSE_Z);

	//Bloom buffer
	//No need for a full-screen bloom, but the length and width are multiples of 8 and the number of cs scheduling threads match
	//Divisible by 128 so that after dividing by 16, we still have multiples of 8x8 tiles. Look at the CS shader to understand why it is 128
	uint32_t BloomWidth = Math::AlignUp(NativeWidth / 2, 128);
	uint32_t BloomHeight = Math::AlignUp(NativeHeight / 2, 128);
	g_LumaBloom.Create(L"Luminance Buffer", BloomWidth, BloomHeight, 1, DXGI_FORMAT_R8_UINT);
	// 1/2 
	g_aBloomUAV1[0].Create(L"Bloom Buffer 1a", BloomWidth, BloomHeight, 1, DefaultHDRColorFormat);
	g_aBloomUAV1[1].Create(L"Bloom Buffer 1b", BloomWidth, BloomHeight, 1, DefaultHDRColorFormat);
	// 1/4
	g_aBloomUAV2[0].Create(L"Bloom Buffer 2a", BloomWidth / 2, BloomHeight / 2, 1, DefaultHDRColorFormat);
	g_aBloomUAV2[1].Create(L"Bloom Buffer 2b", BloomWidth / 2, BloomHeight / 2, 1, DefaultHDRColorFormat);
	// 1/8
	g_aBloomUAV3[0].Create(L"Bloom Buffer 3a", BloomWidth / 4, BloomHeight / 4, 1, DefaultHDRColorFormat);
	g_aBloomUAV3[1].Create(L"Bloom Buffer 3b", BloomWidth / 4, BloomHeight / 4, 1, DefaultHDRColorFormat);
	// 1/16
	g_aBloomUAV4[0].Create(L"Bloom Buffer 4a", BloomWidth / 8, BloomHeight / 8, 1, DefaultHDRColorFormat);
	g_aBloomUAV4[1].Create(L"Bloom Buffer 4b", BloomWidth / 8, BloomHeight / 8, 1, DefaultHDRColorFormat);
	// 1/32
	g_aBloomUAV5[0].Create(L"Bloom Buffer 5a", BloomWidth / 16, BloomHeight / 16, 1, DefaultHDRColorFormat);
	g_aBloomUAV5[1].Create(L"Bloom Buffer 5b", BloomWidth / 16, BloomHeight / 16, 1, DefaultHDRColorFormat);

	g_LumaBuffer.Create(L"Luminance", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R8_UNORM);

	//Linear Depth
	g_LinearDepth.Create(L"Linear Depth", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16_UNORM);

	g_SSAOFullScreen.Create(L"SSAO Full Resolution", NativeWidth / 2, NativeHeight / 2, 1, DXGI_FORMAT_R8_UNORM);

	g_PostColorBuffer.Create(L"Post Color Buffer", NativeWidth, NativeHeight, 1, DefaultHDRColorFormat);

	g_BlurTemp_HalfBuffer_R8.Create(L"Blur Half Temporary Buffer(R8_UNORM)", NativeWidth / 2, NativeHeight / 2, 1, DXGI_FORMAT_R8_UNORM);

	g_VelocityBuffer.Create(L"Motion Vectors", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R32_UINT);

	g_MotionPrepBuffer.Create(L"Motion Blur Prep", NativeWidth / 2, NativeHeight / 2, 1, HDR_MOTION_FORMAT);

	g_TemporalColor[0].Create(L"Temporal Color 0", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	g_TemporalColor[1].Create(L"Temporal Color 1", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
}

void GlareEngine::DestroyRenderingBuffers()
{
	g_SceneDepthBuffer.Destroy();

	g_SceneColorBuffer.Destroy();

	g_LinearDepth.Destroy();

	g_PostEffectsBuffer.Destroy();

	g_PostColorBuffer.Destroy();

	g_SceneMSAAColorBuffer.Destroy();

	g_SceneMSAADepthBuffer.Destroy();

	g_aBloomUAV1[0].Destroy();

	g_aBloomUAV2[0].Destroy();

	g_aBloomUAV3[0].Destroy();

	g_aBloomUAV4[0].Destroy();

	g_aBloomUAV5[0].Destroy();

	g_aBloomUAV1[1].Destroy();

	g_aBloomUAV2[1].Destroy();

	g_aBloomUAV3[1].Destroy();

	g_aBloomUAV4[1].Destroy();

	g_aBloomUAV5[1].Destroy();

	g_LumaBloom.Destroy();

	g_Histogram.Destroy();

	g_LumaBuffer.Destroy();

	g_SSAOFullScreen.Destroy();

	g_BlurTemp_HalfBuffer_R8.Destroy();

	g_VelocityBuffer.Destroy();

	g_MotionPrepBuffer.Destroy();

	g_TemporalColor[0].Destroy();
	g_TemporalColor[1].Destroy();

	/*g_HorizontalBuffer.Destroy();*/

	//g_ShadowBuffer.Destroy();

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

	//g_FXAAWorkCounters.Destroy();
	//g_FXAAWorkQueue.Destroy();
	//g_FXAAColorQueue.Destroy();

	//g_GenMipsBuffer.Destroy();
}
