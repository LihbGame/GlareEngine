#pragma once
#include <string>
#include "Graphics/CommandContext.h"
#include "Graphics/GPUTimeManager.h"
namespace GlareEngine
{
	class NestedTimingTree;

	class GPUTimer
	{
	public:

		GPUTimer()
		{
			m_TimerIndex = GPUTimeManager::NewTimer();
		}

		void Start(CommandContext& Context)
		{
			GPUTimeManager::StartTimer(Context, m_TimerIndex);
		}

		void Stop(CommandContext& Context)
		{
			GPUTimeManager::StopTimer(Context, m_TimerIndex);
		}

		float GetTime(void)
		{
			return GPUTimeManager::GetTime(m_TimerIndex);
		}

		uint32_t GetTimerIndex(void)
		{
			return m_TimerIndex;
		}
	private:

		uint32_t m_TimerIndex;
	};

	namespace EngineProfiling
	{
		void Update();
		void BeginBlock(const std::wstring& name, CommandContext* Context = nullptr);
		void EndBlock(CommandContext* Context = nullptr);
		bool IsPaused();
		float GetFrameTime();
	};


#ifdef RELEASE
	class ScopedTimer
	{
	public:
		ScopedTimer(const std::wstring&) {}
		ScopedTimer(const std::wstring&, CommandContext&) {}
	};
#else
	class ScopedTimer
	{
	public:
		ScopedTimer(const std::wstring& name) : m_Context(nullptr)
		{
			EngineProfiling::BeginBlock(name);
		}
		ScopedTimer(const std::wstring& name, CommandContext& Context) : m_Context(&Context)
		{
			EngineProfiling::BeginBlock(name, m_Context);
		}
		~ScopedTimer()
		{
			EngineProfiling::EndBlock(m_Context);
		}

	private:
		CommandContext* m_Context;
	};
#endif

}
