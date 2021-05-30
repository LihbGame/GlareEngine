#pragma once
#include <string>
#include "CommandContext.h"

namespace GlareEngine
{
	namespace EngineProfiling
	{
		void Update();

		void BeginBlock(const std::wstring& name, DirectX12Graphics::CommandContext* Context = nullptr);
		void EndBlock(DirectX12Graphics::CommandContext* Context = nullptr);

		bool IsPaused();
	};


#ifdef RELEASE
	class ScopedTimer
	{
	public:
		ScopedTimer(const std::wstring&) {}
		ScopedTimer(const std::wstring&, DirectX12Graphics::CommandContext&) {}
	};
#else
	class ScopedTimer
	{
	public:
		ScopedTimer(const std::wstring& name) : m_Context(nullptr)
		{
			EngineProfiling::BeginBlock(name);
		}
		ScopedTimer(const std::wstring& name, DirectX12Graphics::CommandContext& Context) : m_Context(&Context)
		{
			EngineProfiling::BeginBlock(name, m_Context);
		}
		~ScopedTimer()
		{
			EngineProfiling::EndBlock(m_Context);
		}

	private:
		DirectX12Graphics::CommandContext* m_Context;
	};
#endif

}
