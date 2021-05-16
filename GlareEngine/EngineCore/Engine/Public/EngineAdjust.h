#pragma once
#include <string>
#include <stdint.h>
#include <float.h>
#include <map>
#include <set>


namespace GlareEngine
{
	class VariableGroup;

	class EngineVar
	{
	public:

		virtual ~EngineVar() {}

		virtual void Increment(void) {}    // DPad Right
		virtual void Decrement(void) {}    // DPad Left
		virtual void Bang(void) {}        // A Button

		virtual std::string ToString(void) const { return ""; }
		virtual void SetValue(FILE* file, const std::string& setting) = 0; //set value read from file

		EngineVar* NextVar(void);
		EngineVar* PrevVar(void);

	protected:
		EngineVar(void);
		EngineVar(const std::string& path);

	private:
		
		VariableGroup* m_GroupPtr;
		friend class VariableGroup;
	};


	class BoolVar : public EngineVar
	{
	public:
		BoolVar(const std::string& path, bool val);
		BoolVar& operator=(bool val) { m_Flag = val; return *this; }
		operator bool() const { return m_Flag; }

		virtual void Increment(void) override { m_Flag = true; }
		virtual void Decrement(void) override { m_Flag = false; }
		virtual void Bang(void) override { m_Flag = !m_Flag; }

		virtual std::string ToString(void) const override;
		virtual void SetValue(FILE* file, const std::string& setting) override;

	private:
		bool m_Flag;
	};

	class NumVar : public EngineVar
	{
	public:
		NumVar(const std::string& path, float val, float minValue = -FLT_MAX, float maxValue = FLT_MAX, float stepSize = 1.0f);
		NumVar& operator=(float val) { m_Value = Clamp(val); return *this; }
		operator float() const { return m_Value; }

		virtual void Increment(void) override { m_Value = Clamp(m_Value + m_StepSize); }
		virtual void Decrement(void) override { m_Value = Clamp(m_Value - m_StepSize); }

		virtual std::string ToString(void) const override;
		virtual void SetValue(FILE* file, const std::string& setting)  override;

	protected:
		float Clamp(float val) { return val > m_MaxValue ? m_MaxValue : val < m_MinValue ? m_MinValue : val; }

		float m_Value;
		float m_MinValue;
		float m_MaxValue;
		float m_StepSize;
	};

}