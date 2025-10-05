#include <vector>
#include <unordered_map>
#include <array>
#include "EngineProfiling.h"
#include "GameTimer.h"
#include "Graphics/GraphicsCore.h"
#include "EngineInput.h"
#include "Graphics/CommandContext.h"
#include "EngineAdjust.h"
#include "EngineLog.h"
#include "Engine/Scene.h"
#include "SystemTime.h"

using namespace GlareEngine;
using namespace GlareEngine::Math;

namespace GlareEngine
{
	namespace EngineProfiling
	{
		bool Paused = false;
	};


	//记录每帧的状态
	class StatHistory
	{
	public:
		StatHistory()
		{
			for (uint32_t i = 0; i < kHistorySize; ++i)
				m_RecentHistory[i] = 0.0f;
			for (uint32_t i = 0; i < kExtendedHistorySize; ++i)
				m_ExtendedHistory[i] = 0.0f;
			m_Average = 0.0f;
			m_Minimum = 0.0f;
			m_Maximum = 0.0f;
		}

		void RecordStat(uint32_t FrameIndex, float Value)
		{
			m_RecentHistory[FrameIndex % kHistorySize] = Value;
			m_ExtendedHistory[FrameIndex % kExtendedHistorySize] = Value;
			m_Recent = Value;

			uint32_t ValidCount = 0;
			m_Minimum = FLT_MAX;
			m_Maximum = 0.0f;
			m_Average = 0.0f;

			for (float val : m_RecentHistory)
			{
				if (val > 0.0f)
				{
					++ValidCount;
					m_Average += val;
					m_Minimum = min(val, m_Minimum);
					m_Maximum = max(val, m_Maximum);
				}
			}

			if (ValidCount > 0)
				m_Average /= (float)ValidCount;
			else
				m_Minimum = 0.0f;
		}

		float GetLast(void) const { return m_Recent; }
		float GetMax(void) const { return m_Maximum; }
		float GetMin(void) const { return m_Minimum; }
		float GetAvg(void) const { return m_Average; }

		const float* GetHistory(void) const { return m_ExtendedHistory; }
		uint32_t GetHistoryLength(void) const { return kExtendedHistorySize; }

	private:
		static const uint32_t kHistorySize = 64;
		static const uint32_t kExtendedHistorySize = 256;
		float m_RecentHistory[kHistorySize];
		float m_ExtendedHistory[kExtendedHistorySize];
		float m_Recent;
		float m_Average;
		float m_Minimum;
		float m_Maximum;
	};


	//递归时序树
	class NestedTimingTree
	{
	public:
		NestedTimingTree(const wstring& name, NestedTimingTree* parent = nullptr)
			: m_Name(name), m_Parent(parent), m_IsExpanded(false) {}

		~NestedTimingTree()
		{
			DeleteChildren();
		}

		NestedTimingTree* GetChild(const wstring& name)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			auto iter = m_LUT.find(name);
			if (iter != m_LUT.end())
				return iter->second;

			NestedTimingTree* node = new NestedTimingTree(name, this);
			m_Children.push_back(node);
			m_LUT[name] = node;
			return node;
		}

		NestedTimingTree* NextScope(void)
		{
			if (m_IsExpanded && m_Children.size() > 0)
				return m_Children[0];

			return m_Parent->NextChild(this);
		}

		NestedTimingTree* PrevScope(void)
		{
			NestedTimingTree* prev = m_Parent->PrevChild(this);
			return prev == m_Parent ? prev : prev->LastChild();
		}

		NestedTimingTree* FirstChild(void)
		{
			return m_Children.size() == 0 ? nullptr : m_Children[0];
		}

		NestedTimingTree* LastChild(void)
		{
			if (!m_IsExpanded || m_Children.size() == 0)
				return this;

			return m_Children.back()->LastChild();
		}

		NestedTimingTree* NextChild(NestedTimingTree* curChild)
		{
			assert(curChild->m_Parent == this);

			for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
			{
				if (*iter == curChild)
				{
					auto nextChild = iter; ++nextChild;
					if (nextChild != m_Children.end())
						return *nextChild;
				}
			}

			if (m_Parent != nullptr)
				return m_Parent->NextChild(this);
			else
				return &sm_RootScope;
		}

		NestedTimingTree* PrevChild(NestedTimingTree* curChild)
		{
			assert(curChild->m_Parent == this);

			if (*m_Children.begin() == curChild)
			{
				if (this == &sm_RootScope)
					return sm_RootScope.LastChild();
				else
					return this;
			}

			for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
			{
				if (*iter == curChild)
				{
					auto prevChild = iter; --prevChild;
					return *prevChild;
				}
			}

			EngineLog::AddLog(L"Error: All attempts to find a previous timing sample failed");
			return nullptr;
		}

		void StartTiming(CommandContext* Context)
		{
			m_StartTick = SystemTime::GetCurrentTick();
			if (Context == nullptr)
				return;
			m_GPUTimer.Start(*Context);

			Context->PIXBeginEvent(m_Name.c_str());
		}

		void StopTiming(CommandContext* Context)
		{
			m_EndTick = SystemTime::GetCurrentTick();
			if (Context == nullptr)
				return;
			m_GPUTimer.Stop(*Context);

			Context->PIXEndEvent();
		}

		void GatherTimes(uint32_t FrameIndex)
		{
			if (EngineProfiling::Paused)
			{
				for (auto node : m_Children)
					node->GatherTimes(FrameIndex);
				return;
			}
			m_CPUTime.RecordStat(FrameIndex, 1000.0f * (float)SystemTime::TimeBetweenTicks(m_StartTick, m_EndTick));
			m_GPUTime.RecordStat(FrameIndex, 1000.0f * m_GPUTimer.GetTime());

			for (auto node : m_Children)
				node->GatherTimes(FrameIndex);

			m_StartTick = 0;
			m_EndTick = 0;
		}

		void SumInclusiveTimes(float& cpuTime, float& gpuTime)
		{
			cpuTime = 0.0f;
			gpuTime = 0.0f;
			for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
			{
				cpuTime += (*iter)->m_CPUTime.GetLast();
				gpuTime += (*iter)->m_GPUTime.GetLast();
			}
		}

		static void PushProfilingMarker(const wstring& name, CommandContext* Context);
		static void PopProfilingMarker(CommandContext* Context);
		static void Update(void);
		static void UpdateTimes(void)
		{
			if (EngineGlobal::gCurrentScene->LoadingFinish)
			{
				uint32_t FrameIndex = (uint32_t)GlareEngine::Display::GetFrameCount();

				m_CPUStartTick = m_CPUEndTick;
				m_CPUEndTick = SystemTime::GetCurrentTick();

				//开始GPU Time Buffer 的读取
				GPUTimeManager::BeginReadBack();
				//统计时间树的所有结点的时间
				sm_RootScope.GatherTimes(FrameIndex);
				//记录每帧的时间timer的0-1记录了本帧的时间
				s_FrameDelta.RecordStat(FrameIndex, GPUTimeManager::GetTime(0));
				//结束GPU Time Buffer 的读取
				GPUTimeManager::EndReadBack();

				float TotalCPUTime, TotalGPUTime;
				//统计一帧的GPU,CPU总耗费时间
				sm_RootScope.SumInclusiveTimes(TotalCPUTime, TotalGPUTime);
				//记录CPU和GPU总耗费时间
				s_AvgCPUTime.RecordStat(FrameIndex, (float)SystemTime::TimeBetweenTicks(m_CPUStartTick, m_CPUEndTick));
				s_TotalCPUTime.RecordStat(FrameIndex, TotalCPUTime);
				s_TotalGPUTime.RecordStat(FrameIndex, TotalGPUTime);
			}
		}

		static float GetAvgCPUTime(void) { return  s_AvgCPUTime.GetAvg(); }
		static float GetTotalCPUTime(void) { return s_TotalCPUTime.GetAvg(); }
		static float GetTotalGPUTime(void) { return s_TotalGPUTime.GetAvg(); }
		static float GetFrameDelta(void) { return s_FrameDelta.GetAvg(); }
	private:

		void DeleteChildren(void)
		{
			for (auto node : m_Children)
				delete node;
			m_Children.clear();
		}
		std::mutex m_Mutex;
		wstring m_Name;
		NestedTimingTree* m_Parent;
		vector<NestedTimingTree*> m_Children;
		unordered_map<wstring, NestedTimingTree*> m_LUT;
		int64_t m_StartTick;
		int64_t m_EndTick;
		StatHistory m_CPUTime;
		StatHistory m_GPUTime;
		bool m_IsExpanded;
		GPUTimer m_GPUTimer;
		static int64_t m_CPUStartTick;
		static int64_t m_CPUEndTick;
		static StatHistory s_AvgCPUTime;
		static StatHistory s_TotalCPUTime;
		static StatHistory s_TotalGPUTime;
		static StatHistory s_FrameDelta;
		static NestedTimingTree sm_RootScope;
		static NestedTimingTree* sm_CurrentNode;
		static NestedTimingTree* sm_SelectedScope;

		static bool sm_CursorOnGraph;

	};
	int64_t NestedTimingTree::m_CPUStartTick = 0;
	int64_t NestedTimingTree::m_CPUEndTick = 0;
	StatHistory NestedTimingTree::s_AvgCPUTime;
	StatHistory NestedTimingTree::s_TotalCPUTime;
	StatHistory NestedTimingTree::s_TotalGPUTime;
	StatHistory NestedTimingTree::s_FrameDelta;
	NestedTimingTree NestedTimingTree::sm_RootScope(L"TotalGPU");
	NestedTimingTree* NestedTimingTree::sm_CurrentNode = &NestedTimingTree::sm_RootScope;
	namespace EngineProfiling
	{
		BoolVar DrawFrameRate("Display Frame Rate", true);
		BoolVar DrawProfiler("Display Profiler", false);

		void Update(void)
		{
			if (EngineInput::IsFirstPressed(EngineInput::kStartButton)
				|| EngineInput::IsFirstPressed(EngineInput::kKey_Space))
			{
				Paused = !Paused;
			}
			NestedTimingTree::UpdateTimes();
		}

		void BeginBlock(const wstring& name, CommandContext* Context)
		{
			NestedTimingTree::PushProfilingMarker(name, Context);
		}

		void EndBlock(CommandContext* Context)
		{
			NestedTimingTree::PopProfilingMarker(Context);
		}

		bool IsPaused()
		{
			return Paused;
		}

		float GetCPUFrameTime()
		{
			return NestedTimingTree::GetAvgCPUTime();
		}

		float GetGPUFrameTime()
		{
			return NestedTimingTree::GetFrameDelta();
		}
	} // EngineProfiling

	void NestedTimingTree::PushProfilingMarker(const wstring& name, CommandContext* Context)
	{
		sm_CurrentNode = sm_CurrentNode->GetChild(name);
		sm_CurrentNode->StartTiming(Context);
	}

	void NestedTimingTree::PopProfilingMarker(CommandContext* Context)
	{
		sm_CurrentNode->StopTiming(Context);
		sm_CurrentNode = sm_CurrentNode->m_Parent;
	}

	void NestedTimingTree::Update(void)
	{
	}
}