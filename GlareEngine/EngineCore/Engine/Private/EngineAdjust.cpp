#include "L3DUtil.h"
#include "EngineAdjust.h"
#include "EngineInput.h"
#include "Color.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

using namespace std;
using namespace GlareEngine;
using namespace GlareEngine::Math;
using namespace GlareEngine::DirectX12Graphics;

namespace GlareEngine
{
	namespace EngineAdjust
	{
		//延迟注册。在将一些对象添加到图之前，
		//已经构造了一些对象(由于初始化顺序不可靠)。 
		enum { kMaxUnregisteredTweaks = 1024 };
		char s_UnregisteredPath[kMaxUnregisteredTweaks][128];
		EngineVar* s_UnregisteredVariable[kMaxUnregisteredTweaks] = { nullptr };
		int32_t s_UnregisteredCount = 0;

		// Internal functions
		void AddToVariableGraph(const string& path, EngineVar& var);
		void RegisterVariable(const string& path, EngineVar& var);

		EngineVar* sm_SelectedVariable = nullptr;
		bool sm_IsVisible = false;
	}
}


//不向公众开放。当调节器的路径包含组名时，将自动创建组。 
class VariableGroup : public EngineVar
{
public:
	VariableGroup() : m_IsExpanded(false) {}

	EngineVar* FindChild(const string& name)
	{
		auto iter = m_Children.find(name);
		return iter == m_Children.end() ? nullptr : iter->second;
	}

	void AddChild(const string& name, EngineVar& child)
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

	virtual void SetValue(FILE*, const std::string&) override {}

	static VariableGroup sm_RootGroup;

private:
	bool m_IsExpanded;
	std::map<string, EngineVar*> m_Children;
};

VariableGroup VariableGroup::sm_RootGroup;



void EngineAdjust::RegisterVariable(const string& path, EngineVar& var)
{
	if (s_UnregisteredCount >= 0)
	{
		int32_t Idx = s_UnregisteredCount++;
		strcpy_s(s_UnregisteredPath[Idx], path.c_str());
		s_UnregisteredVariable[Idx] = &var;
	}
	else
	{
		AddToVariableGraph(path, var);
	}
}


void EngineAdjust::AddToVariableGraph(const string& path, EngineVar& var)
{
	vector<string> SeparatedPath;
	string leafName;
	size_t start = 0, end = 0;

	while (true)
	{
		end = path.find('/', start);
		if (end == string::npos)
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
			nextGroup = new VariableGroup();
			group->AddChild(*iter, *nextGroup);
			group = nextGroup;
		}
		else
		{
			nextGroup = dynamic_cast<VariableGroup*>(node);
			assert(nextGroup != nullptr, "Attempted to trash the tweak graph");
			group = nextGroup;
		}
	}

	group->AddChild(leafName, var);
}

EngineVar* VariableGroup::NextVariable(EngineVar* currentVariable)
{
	auto iter = m_Children.begin();
	for (; iter != m_Children.end(); ++iter)
	{
		if (currentVariable == iter->second)
			break;
	}

	assert(iter != m_Children.end(), "Do not find engine variable in its designated group");

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

	assert(iter != m_Children.end(), "Did not find engine variable in its designated group");

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
