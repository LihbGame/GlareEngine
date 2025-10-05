#pragma once

#include "Engine/GameCore.h"
#include "CommandContext.h"

namespace GlareEngine
{
	namespace GPUTimeManager
	{
		void Initialize(uint32_t MaxNumTimers = 4096);
		void Shutdown();

		//保留唯一的计时器索引 
		uint32_t NewTimer(void);

		//在GPU时间轴上写入开始和结束时间戳记 
		void StartTimer(CommandContext& Context, uint32_t TimerIdx);
		void StopTimer(CommandContext& Context, uint32_t TimerIdx);

		//Map/Unmap相对应的Begin/End预订对GetTime()的所有调用。
		//这需要在帧的开始或结束时发生。 
		void BeginReadBack(void);
		void EndReadBack(void);

		//返回开始和停止查询之间的时间(以毫秒为单位) 
		float GetTime(uint32_t TimerIdx);

		int32_t GetTimeCount();
		ID3D12QueryHeap* GetTimeHeap();
	};
}

