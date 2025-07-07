#pragma once
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "ShadowBuffer.h"
#include "GPUBuffer.h"
#include "GraphicsCore.h"
#include "Render.h"


/// <summary>
/// Define
/// </summary>

#define  MSAACOUNT					4

#define T2X_COLOR_FORMAT			DXGI_FORMAT_R10G10B10A2_UNORM

#define HDR_MOTION_FORMAT			DXGI_FORMAT_R16G16B16A16_FLOAT

#define DSV_FORMAT					DXGI_FORMAT_R32_TYPELESS


/// <summary>
/// Global Buffer
/// </summary>

namespace GlareEngine
{
	extern DXGI_FORMAT				DefaultHDRColorFormat;			//DXGI_FORMAT_R11G11B10_FLOAT

	extern DepthBuffer				g_SceneDepthBuffer;				// D32_FLOAT_S8_UINT

	extern ColorBuffer				g_SceneColorBuffer;				// R11G11B10_FLOAT

	extern ColorBuffer				g_PostEffectsBuffer;			// R32_UINT (To support Read-Modify-Write with a UAV)	

	extern ColorBuffer				g_PostColorBuffer;			    //ping-pong with main scene RT in post process chain

	extern ColorBuffer				g_SceneMSAAColorBuffer;

	extern DepthBuffer				g_SceneMSAADepthBuffer;

	extern ColorBuffer				g_aBloomUAV1[2];   

	extern ColorBuffer				g_aBloomUAV2[2];   

	extern ColorBuffer				g_aBloomUAV3[2];  

	extern ColorBuffer				g_aBloomUAV4[2];  

	extern ColorBuffer				g_aBloomUAV5[2];    

	extern ColorBuffer				g_LumaBloom;
					
	extern ByteAddressBuffer		g_Histogram;

	extern ColorBuffer				g_LumaBuffer;

	extern ColorBuffer				g_LinearDepth[2];				// Normalized planar distance [0 at eye, 1 at far plane] computed from the Scene Depth Buffer

	extern ColorBuffer				g_SSAOFullScreen;				// R8_UNORM

	extern ColorBuffer				g_BlurTempBuffer_R8;			// R8_UNORM

	extern ColorBuffer				g_VelocityBuffer;				// R10G10B10  (3D velocity)

	extern ColorBuffer				g_MotionPrepBuffer;				

	extern ColorBuffer				g_TemporalColor[2];

	//GBuffer
	extern ColorBuffer				g_GBuffer[Render::GBufferType::GBUFFER_Count];
	

	//extern ColorBuffer g_OverlayBuffer;        // R8G8B8A8_UNORM
	//extern ColorBuffer g_HorizontalBuffer;    // For separable (bicubic) upsampling


	//extern ShadowBuffer g_ShadowBuffer;


	//extern ColorBuffer g_MinMaxDepth8;        // Min and max depth values of 8x8 tiles
	//extern ColorBuffer g_MinMaxDepth16;        // Min and max depth values of 16x16 tiles
	//extern ColorBuffer g_MinMaxDepth32;        // Min and max depth values of 16x16 tiles
	//extern ColorBuffer g_DepthDownsize1;
	//extern ColorBuffer g_DepthDownsize2;
	//extern ColorBuffer g_DepthDownsize3;
	//extern ColorBuffer g_DepthDownsize4;
	//extern ColorBuffer g_DepthTiled1;
	//extern ColorBuffer g_DepthTiled2;
	//extern ColorBuffer g_DepthTiled3;
	//extern ColorBuffer g_DepthTiled4;

	//extern ColorBuffer g_DoFTileClass[2];
	//extern ColorBuffer g_DoFPresortBuffer;
	//extern ColorBuffer g_DoFPrefilter;
	//extern ColorBuffer g_DoFBlurColor[2];
	//extern ColorBuffer g_DoFBlurAlpha[2];
	//extern StructuredBuffer g_DoFWorkQueue;
	//extern StructuredBuffer g_DoFFastQueue;
	//extern StructuredBuffer g_DoFFixupQueue;
	

	void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);

	void ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight);

	void ResizeDisplayBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	

	void DestroyRenderingBuffers();

}// GlareEngine
