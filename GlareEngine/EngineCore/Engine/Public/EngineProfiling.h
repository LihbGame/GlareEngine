#pragma once
namespace GlareEngine
{
	class EngineProfiling
	{
		void Update();

		void BeginBlock(const std::wstring& name, CommandContext* Context = nullptr);
		void EndBlock(CommandContext* Context = nullptr);

		bool IsPaused();
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
