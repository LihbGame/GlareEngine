#include "GameCore.h"
#include "GraphicsCore.h"
#include "L3DGameTimer.h"
#include "EngineInput.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "EngineAdjust.h"
#include "EngineProfiling.h"

#pragma comment(lib, "runtimeobject.lib")

namespace GlareEngine
{
	namespace DirectX12Graphics
	{
		extern ColorBuffer g_GenMipsBuffer;
	}
}



namespace GlareEngine
{
	namespace GameCore
	{
		using namespace GlareEngine::DirectX12Graphics;

		void InitializeApplication(GameApp& Game)
		{
			DirectX12Graphics::Initialize();
			EngineInput::Initialize();
			EngineAdjust::Initialize();

			Game.Startup();
		}

		void TerminateApplication(GameApp& Game)
		{
			Game.Cleanup();
			EngineInput::Shutdown();
		}

		bool UpdateApplication(GameApp& Game)
		{
			EngineProfiling::Update();

			float DeltaTime = DirectX12Graphics::GetFrameTime();

			EngineInput::Update(DeltaTime);
			EngineAdjust::Update(DeltaTime);

			Game.Update(DeltaTime);
			Game.RenderScene();
			//Game.RenderUI();
			//PostEffects::Render();

			DirectX12Graphics::Present();

			return !Game.IsDone();
		}


	}
}