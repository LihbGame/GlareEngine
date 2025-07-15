#pragma once

#include <cstdint>
#include "ColorBuffer.h"
namespace GlareEngine
{
    namespace Display
    {
        extern uint32_t g_DisplayWidth;
        extern uint32_t g_DisplayHeight;
		extern uint32_t g_RenderWidth;
		extern uint32_t g_RenderHeight;

		extern float g_UpscaleRatio;
        extern bool  g_bUpscale;

        extern bool g_bEnableHDROutput;
        extern IDXGISwapChain1* s_SwapChain1;
        extern NumVar g_HDRPaperWhite;
        extern NumVar g_MaxDisplayLuminance;


        // Returns the number of elapsed frames since application start
        uint64_t GetFrameCount(void);

        // The amount of time elapsed during the last completed frame.The CPU and/or
        // GPU may be idle during parts of the frame.The frame time measures the time
        // between calls to present each frame.
        float GetFrameTime(void);

        // The total number of frames per second
        float GetFrameRate(void);
 
        void Initialize(void);
        void Shutdown(void);
        void Present(void);
        void Resize(uint32_t width, uint32_t height);
        void PreparePresent();

        ColorBuffer& GetCurrentBuffer();
        ColorBuffer& GetCurrentSceneColorBufferBuffer();
    };

}