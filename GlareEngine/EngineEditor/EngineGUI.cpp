#include "EngineGUI.h"
#include <dxgi.h>
#include "L3DUtil.h"
using Microsoft::WRL::ComPtr;
EngineGUI::EngineGUI(ID3D12Device* d3dDevice)
{
	pd3dDevice = d3dDevice;
}

EngineGUI::~EngineGUI()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void EngineGUI::InitGUI(HWND GameWnd,ID3D12DescriptorHeap* GUISrvDescriptorHeap)
{
	mGUISrvDescriptorHeap = GUISrvDescriptorHeap;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls



	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();
	 // Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(GameWnd);
	ImGui_ImplDX12_Init(pd3dDevice, gNumFrameResources,
		DXGI_FORMAT_R8G8B8A8_UNORM, GUISrvDescriptorHeap,
		GUISrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		GUISrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());



	// Load Fonts
   // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
   // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
   // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
   // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
   // - Read 'docs/FONTS.txt' for more instructions and details.
   // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
   //io.Fonts->AddFontDefault();
   io.Fonts->AddFontFromFileTTF("UImisc/fonts/Roboto-Medium.ttf", 16.0f);
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("UImisc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("misc/fonts/Roboto-Medium.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);

	 g = ImGui::GetCurrentContext();
	 g->Style.WindowRounding = 0;
}

void EngineGUI::CreateUIDescriptorHeap(ComPtr<ID3D12DescriptorHeap>& DescriptorHeap)
{
	
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(DescriptorHeap.GetAddressOf())));
}

void EngineGUI::DrawUI(ID3D12GraphicsCommandList* d3dCommandList)
{
	//DRAW GUI
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { mGUISrvDescriptorHeap };
		d3dCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		// Our state

		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
		{
			g->NextWindowData.MenuBarOffsetMinVal = ImVec2(g->Style.DisplaySafeAreaPadding.x, ImMax(g->Style.DisplaySafeAreaPadding.y - g->Style.FramePadding.y, 0.0f));
			ImGui::SetNextWindowPos(ImVec2(g->IO.DisplaySize.x * 5.0f / 6.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x * 1.0f/6.0f, g->IO.DisplaySize.y * 0.75f));
			ImGui::ShowDemoWindow(&show_demo_window);

		}
		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static int counter = 0;

			ImGui::SetNextWindowPos(ImVec2(0.0f, MainMenuBarHeight));
			ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x * 1.0f/6.0f, g->IO.DisplaySize.y*0.75f- MainMenuBarHeight));
			
			ImGui::Begin("Hello, world!",&helloWindow,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);                          // Create a window called "Hello, world!" and append into it.
			
			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			
			ImGui::Text("Camera Move Speed");
			ImGui::SliderFloat("   ", &CameraMoveSpeed, 0.0f, 100.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

			ImGui::Text("Show Sence");
			ImGui::Checkbox("Shadow", &show_shadow);
			ImGui::Checkbox("Model", &show_model);
			ImGui::Checkbox("Sky", &show_sky);
			ImGui::Checkbox("Land", &show_land);
			ImGui::Checkbox("Water", &show_water);
			ImGui::Checkbox("HeightMapTerrain", &show_HeightMapTerrain);
			ImGui::Checkbox("Grass", &show_Grass);
			ImGui::SliderFloat("MinWind", &GrassMinWind, 0.0f, 1.0f);
			ImGui::SliderFloat("MaxWind", &GrassMaxWind, 1.0f, 4.0f);
			ImGui::Checkbox("Fog", &FogEnabled);
			ImGui::Text("Fog Start");
			ImGui::SliderFloat(" ", &FogStart, 0.0f, 1000.0f);
			ImGui::Text("Fog Range");
			ImGui::SliderFloat("  ", &FogRange, 0.0f, 1000.0f);
			//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (show_another_window)
		{

			//g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
			ImGui::SetNextWindowPos(ImVec2(0.0f, g->IO.DisplaySize.y * 0.75f));
			ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x, g->IO.DisplaySize.y * 0.25f));

			ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				show_another_window = false;
			ImGui::End();
		}
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3dCommandList);
	}
}

