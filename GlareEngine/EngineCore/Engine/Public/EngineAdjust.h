#pragma once
#include <string>
#include <stdint.h>
#include <float.h>
#include <map>
#include <set>

class VariableGroup;


namespace GlareEngine
{
	class EngineVar
	{
	public:

		virtual ~EngineVar() {}

		virtual void Increment(void) {}    // DPad Right
		virtual void Decrement(void) {}    // DPad Left
		virtual void Bang(void) {}        // A Button

		virtual void DisplayValue(TextContext&) const {}
		virtual std::string ToString(void) const { return ""; }
		virtual void SetValue(FILE* file, const std::string& setting) = 0; //set value read from file

		EngineVar* NextVar(void);
		EngineVar* PrevVar(void);

	protected:
		EngineVar(void);
		EngineVar(const std::string& path);

	private:
		friend class VariableGroup;
		VariableGroup* m_GroupPtr;
	};

}