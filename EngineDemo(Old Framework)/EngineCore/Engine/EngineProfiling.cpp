#include <vector>
#include <unordered_map>
#include <array>
#include "EngineProfiling.h"
#include "GameTimer.h"
#include "GraphicsCore.h"
#include "GPUTimeManager.h"
#include "CommandContext.h"
#include "EngineAdjust.h"
#include "EngineLog.h"


using namespace GlareEngine;
using namespace GlareEngine::Math;
using namespace GlareEngine::DirectX12Graphics;

namespace GlareEngine
{
	namespace EngineProfiling
	{
		bool Paused = false;
	};


	//��¼ÿ֡��״̬
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


	class GPUTimer
	{
	public:

		GPUTimer()
		{
			m_TimerIndex = GPUTimeManager::NewTimer();
		}

		void Start(DirectX12Graphics::CommandContext& Context)
		{
			GPUTimeManager::StartTimer(Context, m_TimerIndex);
		}

		void Stop(DirectX12Graphics::CommandContext& Context)
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

	//�ݹ�ʱ����
	class NestedTimingTree
	{
	public:
		NestedTimingTree(const std::wstring& name, NestedTimingTree* parent = nullptr)
			: m_Name(name), m_Parent(parent), m_IsExpanded(false) {}

		NestedTimingTree* GetChild(const std::wstring& name)
		{
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

		void StartTiming(DirectX12Graphics::CommandContext* Context)
		{
			m_StartTick = (int64_t)GameTimer::TotalTime();
			if (Context == nullptr)
				return;

			m_GPUTimer.Start(*Context);
		}

		void StopTiming(DirectX12Graphics::CommandContext* Context)
		{
			m_EndTick = (int64_t)GameTimer::TotalTime();
			if (Context == nullptr)
				return;

			m_GPUTimer.Stop(*Context);
		}

		void GatherTimes(uint32_t FrameIndex)
		{
			if (EngineProfiling::Paused)
			{
				for (auto node : m_Children)
					node->GatherTimes(FrameIndex);
				return;
			}
			m_CPUTime.RecordStat(FrameIndex, 1000.0f * (float)(m_EndTick - m_StartTick));
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

		static void PushProfilingMarker(const std::wstring& name, DirectX12Graphics::CommandContext* Context);
		static void PopProfilingMarker(DirectX12Graphics::CommandContext* Context);
		static void Update(void);
		static void UpdateTimes(void)
		{
			uint32_t FrameIndex = (uint32_t)GlareEngine::Display::GetFrameCount();

			//��ʼGPU Time Buffer �Ķ�ȡ
			GPUTimeManager::BeginReadBack();
			//ͳ��ʱ���������н���ʱ��
			sm_RootScope.GatherTimes(FrameIndex);
			//��¼ÿ֡��ʱ��timer��0-1��¼�˱�֡��ʱ��
			s_FrameDelta.RecordStat(FrameIndex, GPUTimeManager::GetTime(0));
			//����GPU Time Buffer �Ķ�ȡ
			GPUTimeManager::EndReadBack();

			float TotalCPUTime, TotalGPUTime;
			//ͳ��һ֡��GPU,CPU�ܺķ�ʱ��
			sm_RootScope.SumInclusiveTimes(TotalCPUTime, TotalGPUTime);
			//��¼CPU��GPU�ܺķ�ʱ��
			s_TotalCPUTime.RecordStat(FrameIndex, TotalCPUTime);
			s_TotalGPUTime.RecordStat(FrameIndex, TotalGPUTime);
		}

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

		std::wstring m_Name;
		NestedTimingTree* m_Parent;
		std::vector<NestedTimingTree*> m_Children;
		std::unordered_map<std::wstring, NestedTimingTree*> m_LUT;
		int64_t m_StartTick;
		int64_t m_EndTick;
		StatHistory m_CPUTime;
		StatHistory m_GPUTime;
		bool m_IsExpanded;
		GPUTimer m_GPUTimer;
		static StatHistory s_TotalCPUTime;
		static StatHistory s_TotalGPUTime;
		static StatHistory s_FrameDelta;
		static NestedTimingTree sm_RootScope;
		static NestedTimingTree* sm_CurrentNode;
		static NestedTimingTree* sm_SelectedScope;

		static bool sm_CursorOnGraph;

	};



	StatHistory NestedTimingTree::s_TotalCPUTime;
	StatHistory NestedTimingTree::s_TotalGPUTime;
	StatHistory NestedTimingTree::s_FrameDelta;
	NestedTimingTree NestedTimingTree::sm_RootScope(L"");
	NestedTimingTree* NestedTimingTree::sm_CurrentNode = &NestedTimingTree::sm_RootScope;
	namespace EngineProfiling
	{
		BoolVar DrawFrameRate(L"Display Frame Rate", true);
		BoolVar DrawProfiler(L"Display Profiler", false);

		void Update(void)
		{
			NestedTimingTree::UpdateTimes();
		}

		void BeginBlock(const std::wstring& name, DirectX12Graphics::CommandContext* Context)
		{
			NestedTimingTree::PushProfilingMarker(name, Context);
		}

		void EndBlock(DirectX12Graphics::CommandContext* Context)
		{
			NestedTimingTree::PopProfilingMarker(Context);
		}

		bool IsPaused()
		{
			return Paused;
		}
	} // EngineProfiling

	void NestedTimingTree::PushProfilingMarker(const std::wstring& name, DirectX12Graphics::CommandContext* Context)
	{
		sm_CurrentNode = sm_CurrentNode->GetChild(name);
		sm_CurrentNode->StartTiming(Context);
	}

	void NestedTimingTree::PopProfilingMarker(DirectX12Graphics::CommandContext* Context)
	{
		sm_CurrentNode->StopTiming(Context);
		sm_CurrentNode = sm_CurrentNode->m_Parent;
	}

	void NestedTimingTree::Update(void)
	{
	}
}