#pragma once

#include "Engine/GameCore.h"
#include "CommandContext.h"

namespace GlareEngine
{
	namespace GPUTimeManager
	{
		void Initialize(uint32_t MaxNumTimers = 4096);
		void Shutdown();

		//����Ψһ�ļ�ʱ������ 
		uint32_t NewTimer(void);

		//��GPUʱ������д�뿪ʼ�ͽ���ʱ����� 
		void StartTimer(CommandContext& Context, uint32_t TimerIdx);
		void StopTimer(CommandContext& Context, uint32_t TimerIdx);

		//Map/Unmap���Ӧ��Begin/EndԤ����GetTime()�����е��á�
		//����Ҫ��֡�Ŀ�ʼ�����ʱ������ 
		void BeginReadBack(void);
		void EndReadBack(void);

		//���ؿ�ʼ��ֹͣ��ѯ֮���ʱ��(�Ժ���Ϊ��λ) 
		float GetTime(uint32_t TimerIdx);

		int32_t GetTimeCount();
		ID3D12QueryHeap* GetTimeHeap();
	};
}

