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