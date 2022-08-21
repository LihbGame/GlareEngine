#include "EngineUtility.h"
#include "EngineAdjust.h"
#include "Color.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

using namespace GlareEngine;
using namespace GlareEngine::Math;

namespace GlareEngine
{

	namespace EngineAdjust
	{
		//�ӳ�ע�ᡣ�ڽ�һЩ������ӵ�ͼ֮ǰ��
		//�Ѿ�������һЩ����(���ڳ�ʼ��˳�򲻿ɿ�)�� 
		enum { kMaxUnregisteredTweaks = 1024 };
		wchar_t s_UnregisteredPath[kMaxUnregisteredTweaks][128];
		EngineVar* s_UnregisteredVariable[kMaxUnregisteredTweaks] = { nullptr };
		int32_t s_UnregisteredCount = 0;

		// Internal functions
		void AddToVariableGraph(const std::wstring& path, EngineVar& var);
		void RegisterVariable(const std::wstring& path, EngineVar& var);
		VariableGroup* DynamicVariable();

		EngineVar* sm_SelectedVariable = nullptr;
		bool sm_IsVisible = false;
		//var group
		std::vector<VariableGroup> g_Var;
	}


	//�����ڿ��š�����������·����������ʱ�����Զ������顣 
	class VariableGroup : public EngineVar
	{
	public:
		VariableGroup() : m_IsExpanded(false) {}


		EngineVar* FindChild(const std::wstring& name)
		{
			auto iter = m_Children.find(name);
			return iter == m_Children.end() ? nullptr : iter->second;
		}

		void AddChild(const std::wstring& name, EngineVar& child)
		{
			m_Children[name] = &child;
			child.m_GroupPtr = this;
		}

		void SaveToFile(FILE* file, int fileMargin);
		void LoadSettingsFromFile(FILE* file);

		EngineVar* NextVariable(EngineVar* currentVariable);
		EngineVar* PrevVariable(EngineVar* currentVariable);
		EngineVar* FirstVariable(void);
		EngineVar* LastVariable(void);

		bool IsExpanded(void) const { return m_IsExpanded; }

		virtual void Increment(void) override { m_IsExpanded = true; }
		virtual void Decrement(void) override { m_IsExpanded = false; }
		virtual void Bang(void) override { m_IsExpanded = !m_IsExpanded; }

		virtual void SetValue(FILE*, const std::wstring&) override {}

		static VariableGroup sm_RootGroup;

	private:
		bool m_IsExpanded;
		std::map<std::wstring, EngineVar*> m_Children;
	};

	VariableGroup VariableGroup::sm_RootGroup;


	VariableGroup* EngineAdjust::DynamicVariable()
	{
		static int index = 0;
		if (g_Var.size() <= index)
		{
			g_Var.resize(index + 100);
		}
		return &g_Var[index++];
	}

	void EngineAdjust::Initialize(void)
	{
		for (int32_t i = 0; i < s_UnregisteredCount; ++i)
		{
			assert(wcslen(s_UnregisteredPath[i]) > 0);
			assert(s_UnregisteredVariable[i] != nullptr);
			AddToVariableGraph(s_UnregisteredPath[i], *s_UnregisteredVariable[i]);
		}
		s_UnregisteredCount = -1;
	}

	void EngineAdjust::Update(float frameTime)
	{
	}

	void EngineAdjust::RegisterVariable(const std::wstring& path, EngineVar& var)
	{
		if (s_UnregisteredCount >= 0)
		{
			int32_t Idx = s_UnregisteredCount++;
			wcscpy_s(s_UnregisteredPath[Idx], path.c_str());
			s_UnregisteredVariable[Idx] = &var;
		}
		else
		{
			AddToVariableGraph(path, var);
		}
	}




	void EngineAdjust::AddToVariableGraph(const std::wstring& path, EngineVar& var)
	{
		std::vector<std::wstring> SeparatedPath;
		std::wstring leafName;
		size_t start = 0, end = 0;

		while (true)
		{
			end = path.find('/', start);
			if (end == std::wstring::npos)
			{
				leafName = path.substr(start);
				break;
			}
			else
			{
				SeparatedPath.push_back(path.substr(start, end - start));
				start = end + 1;
			}
		}

		VariableGroup* group = &VariableGroup::sm_RootGroup;

		for (auto iter = SeparatedPath.begin(); iter != SeparatedPath.end(); ++iter)
		{
			VariableGroup* nextGroup;
			EngineVar* node = group->FindChild(*iter);
			if (node == nullptr)
			{
				nextGroup = DynamicVariable();
				group->AddChild(*iter, *nextGroup);
				group = nextGroup;
			}
			else
			{
				nextGroup = dynamic_cast<VariableGroup*>(node);
				//Attempted to trash the tweak graph
				assert(nextGroup != nullptr);
				group = nextGroup;
			}
		}

		group->AddChild(leafName, var);
	}

	void VariableGroup::SaveToFile(FILE* file, int fileMargin)
	{
		for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
		{
			const wchar_t* buffer = (iter->first).c_str();

			VariableGroup* subGroup = dynamic_cast<VariableGroup*>(iter->second);
			if (subGroup != nullptr)
			{
				fwprintf(file, L"%*c + %s ...\r\n", fileMargin, ' ', buffer);
				subGroup->SaveToFile(file, fileMargin + 5);
			}
			else if (dynamic_cast<CallbackTrigger*>(iter->second) == nullptr)
			{
				fwprintf(file, L"%*c %s:  %s\r\n", fileMargin, ' ', buffer, iter->second->ToWString().c_str());
			}
		}
	}

	void VariableGroup::LoadSettingsFromFile(FILE* file)
	{
		for (auto iter = m_Children.begin(); iter != m_Children.end(); ++iter)
		{
			VariableGroup* subGroup = dynamic_cast<VariableGroup*>(iter->second);
			if (subGroup != nullptr)
			{
				char skippedLines[100];
				fscanf_s(file, "%*s %[^\n]", skippedLines, (int)_countof(skippedLines));
				subGroup->LoadSettingsFromFile(file);
			}
			else
			{
				iter->second->SetValue(file, iter->first);
			}
		}
	}

	EngineVar* VariableGroup::NextVariable(EngineVar* currentVariable)
	{
		auto iter = m_Children.begin();
		for (; iter != m_Children.end(); ++iter)
		{
			if (currentVariable == iter->second)
				break;
		}
		//Do not find engine variable in its designated group
		assert(iter != m_Children.end());

		auto nextIter = iter;
		++nextIter;

		if (nextIter == m_Children.end())
			return m_GroupPtr ? m_GroupPtr->NextVariable(this) : nullptr;
		else
			return nextIter->second;
	}

	EngineVar* VariableGroup::PrevVariable(EngineVar* currentVariable)
	{
		auto iter = m_Children.begin();
		for (; iter != m_Children.end(); ++iter)
		{
			if (currentVariable == iter->second)
				break;
		}
		//Did not find engine variable in its designated group
		assert(iter != m_Children.end());

		if (iter == m_Children.begin())
			return this;

		auto prevIter = iter;
		--prevIter;

		VariableGroup* isGroup = dynamic_cast<VariableGroup*>(prevIter->second);
		if (isGroup && isGroup->IsExpanded())
			return isGroup->LastVariable();

		return prevIter->second;
	}

	EngineVar* VariableGroup::FirstVariable(void)
	{
		return m_Children.size() == 0 ? nullptr : m_Children.begin()->second;
	}

	EngineVar* VariableGroup::LastVariable(void)
	{
		if (m_Children.size() == 0)
			return this;

		auto LastVariable = m_Children.end();
		--LastVariable;

		VariableGroup* isGroup = dynamic_cast<VariableGroup*>(LastVariable->second);
		if (isGroup && isGroup->IsExpanded())
			return isGroup->LastVariable();

		return LastVariable->second;
	}


	//=====================================================================================================================
	// EngineVar implementations

	EngineVar::EngineVar(void) : m_GroupPtr(nullptr)
	{
	}

	EngineVar::EngineVar(const std::wstring& path) : m_GroupPtr(nullptr)
	{
		EngineAdjust::RegisterVariable(path, *this);
	}


	EngineVar* EngineVar::NextVar(void)
	{
		EngineVar* next = nullptr;
		VariableGroup* isGroup = dynamic_cast<VariableGroup*>(this);
		if (isGroup != nullptr && isGroup->IsExpanded())
			next = isGroup->FirstVariable();

		if (next == nullptr)
			next = m_GroupPtr->NextVariable(this);

		return next != nullptr ? next : this;
	}

	EngineVar* EngineVar::PrevVar(void)
	{
		EngineVar* prev = m_GroupPtr->PrevVariable(this);
		if (prev != nullptr && prev != m_GroupPtr)
		{
			VariableGroup* isGroup = dynamic_cast<VariableGroup*>(prev);
			if (isGroup != nullptr && isGroup->IsExpanded())
				prev = isGroup->LastVariable();
		}
		return prev != nullptr ? prev : this;
	}

	//=====================================================================================================================
	// BoolVar implementations

	BoolVar::BoolVar(const std::wstring& path, bool val)
		: EngineVar(path)
	{
		m_Flag = val;
	}

	std::wstring BoolVar::ToWString(void) const
	{
		return m_Flag ? L"on" : L"off";
	}

	void BoolVar::SetValue(FILE* file, const std::wstring& setting)
	{
		std::wstring pattern = L"\n " + setting + L": %s";
		char valstr[6];

		// ���ļ�������������õ�����ƥ�����Ŀ
		fwscanf_s(file, pattern.c_str(), valstr, _countof(valstr));

		// Look for one of the many affirmations
		m_Flag = (
			0 == _stricmp(valstr, "1") ||
			0 == _stricmp(valstr, "on") ||
			0 == _stricmp(valstr, "yes") ||
			0 == _stricmp(valstr, "true"));
	}


	//=====================================================================================================================
	// NumVar implementations

	NumVar::NumVar(const std::wstring& path, float val, float minVal, float maxVal, float stepSize)
		: EngineVar(path)
	{
		assert(minVal <= maxVal);
		m_MinValue = minVal;
		m_MaxValue = maxVal;
		m_Value = Clamp(val);
		m_StepSize = stepSize;
	}

	std::wstring NumVar::ToWString(void) const
	{
		wchar_t buf[128];
		swprintf_s(buf, L"%f", m_Value);
		return buf;
	}

	void NumVar::SetValue(FILE* file, const std::wstring& setting)
	{
		std::wstring scanString = L"\n" + setting + L": %f";
		float valueRead;

		//If we haven't read correctly, just keep m_Value at default value
		if (fwscanf_s(file, scanString.c_str(), &valueRead))
			*this = valueRead;
	}


	//=====================================================================================================================
	// ExpVar implementations

	ExpVar::ExpVar(const std::wstring& path, float val, float minExp, float maxExp, float expStepSize)
		: NumVar(path, log2f(val), minExp, maxExp, expStepSize)
	{
	}

	ExpVar& ExpVar::operator=(float val)
	{
		m_Value = Clamp(log2f(val));
		return *this;
	}

	ExpVar::operator float() const
	{
		return exp2f(m_Value);
	}


	std::wstring ExpVar::ToWString(void) const
	{
		wchar_t buf[128];
		swprintf_s(buf, L"%f", (float)*this);
		return buf;
	}

	void ExpVar::SetValue(FILE* file, const std::wstring& setting)
	{
		std::wstring scanString = L"\n" + setting + L": %f";
		float valueRead;

		//If we haven't read correctly, just keep m_Value at default value
		if (fwscanf_s(file, scanString.c_str(), &valueRead))
			*this = valueRead;
	}

	//=====================================================================================================================
	// IntVar implementations

	IntVar::IntVar(const std::wstring& path, int32_t val, int32_t minVal, int32_t maxVal, int32_t stepSize)
		: EngineVar(path)
	{
		assert(minVal <= maxVal);
		m_MinValue = minVal;
		m_MaxValue = maxVal;
		m_Value = Clamp(val);
		m_StepSize = stepSize;
	}


	std::wstring IntVar::ToWString(void) const
	{
		wchar_t buf[128];
		swprintf_s(buf, L"%d", m_Value);
		return buf;
	}

	void IntVar::SetValue(FILE* file, const std::wstring& setting)
	{
		std::wstring scanString = L"\n" + setting + L": %d";
		int32_t valueRead;

		if (fwscanf_s(file, scanString.c_str(), &valueRead))
			*this = valueRead;
	}


	//=====================================================================================================================
	// EnumVar implementations

	EnumVar::EnumVar(const std::wstring& path, int32_t initialVal, int32_t listLength, const wchar_t** listLabels)
		: EngineVar(path)
	{
		assert(listLength > 0);
		m_EnumLength = listLength;
		m_EnumLabels = listLabels;
		m_Value = Clamp(initialVal);
	}


	std::wstring EnumVar::ToWString(void) const
	{
		return m_EnumLabels[m_Value];
	}

	void EnumVar::SetValue(FILE* file, const std::wstring& setting)
	{
		std::wstring scanString = L"\n" + setting + L": %[^\n]";
		wchar_t valueRead[14];

		if (fwscanf_s(file, scanString.c_str(), valueRead, _countof(valueRead)) == 1)
		{
			std::wstring valueReadStr = valueRead;
			valueReadStr = valueReadStr.substr(0, valueReadStr.length() - 1);

			//if we don't find the string, then leave m_EnumLabes[m_Value] as default
			for (int32_t i = 0; i < m_EnumLength; ++i)
			{
				if (m_EnumLabels[i] == valueReadStr)
				{
					m_Value = i;
					break;
				}
			}
		}

	}

	//=====================================================================================================================
	// CallbackTrigger implementations

	CallbackTrigger::CallbackTrigger(const std::wstring& path, std::function<void(void*)> callback, void* args)
		: EngineVar(path)
	{
		m_Callback = callback;
		m_Arguments = args;
		m_BangDisplay = 0;
	}

	void CallbackTrigger::SetValue(FILE* file, const std::wstring& setting)
	{
		//Skip over setting without reading anything
		std::wstring scanString = L"\n" + setting + L": %[^\n]";
		char skippedLines[100];
		fwscanf_s(file, scanString.c_str(), skippedLines, _countof(skippedLines));
	}

}


