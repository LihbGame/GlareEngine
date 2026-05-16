#pragma once

#include "Engine/GameCore.h"
#include "CommandContext.h"

namespace GlareEngine
{
    namespace GPUTimeManager
    {
        void Initialize(uint32_t MaxNumTimers = 4096);
        void Shutdown();

        // Reserve a unique timer index
        uint32_t NewTimer(void);

        // Write start and stop time stamps on the GPU timeline
        void StartTimer(CommandContext& Context, uint32_t TimerIdx);
        void StopTimer(CommandContext& Context, uint32_t TimerIdx);

        // Map/Unmap to bracket all calls to GetTime().
        // Needs to happen at the very start or end of a frame.
        void BeginReadBack(void);
        void EndReadBack(void);
        void Resolve(void);

        // Returns the time in milliseconds between start and stop queries
        float GetTime(uint32_t TimerIdx);

        int32_t GetTimeCount();
        ID3D12QueryHeap* GetTimeHeap();
    }
}
