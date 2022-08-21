#pragma once

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
		void StartTimer(DirectX12Graphics::CommandContext& Context, uint32_t TimerIdx);
		void StopTimer(DirectX12Graphics::CommandContext& Context, uint32_t TimerIdx);

		//Map/Unmap���Ӧ��Begin/EndԤ����GetTime()�����е��á�
		//����Ҫ��֡�Ŀ�ʼ�����ʱ������ 
		void BeginReadBack(void);
		void EndReadBack(void);

		//���ؿ�ʼ��ֹͣ��ѯ֮���ʱ��(�Ժ���Ϊ��λ) 
		float GetTime(uint32_t TimerIdx);
	};
}

