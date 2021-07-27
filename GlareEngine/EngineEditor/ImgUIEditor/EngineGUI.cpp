#include "EngineGUI.h"
#include <dxgi.h>
#include "EngineUtility.h"
#include "EngineLog.h"
#include "EngineAdjust.h"
using Microsoft::WRL::ComPtr;
bool gFullSreenMode = false;
bool EngineGUI::mWindowMaxSize = false;
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
   io.Fonts->AddFontFromFileTTF("../GlareEngine/UImisc/fonts/Roboto-Medium.ttf", 16.0f);
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("UImisc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("misc/fonts/Roboto-Medium.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);

	 g = ImGui::GetCurrentContext();
	 g->Style.WindowRounding = 0;

	 SetWindowStyles();
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

		// Start the Dear ImGui frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::SetNextWindowBgAlpha(1);

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
		{
			g->NextWindowData.MenuBarOffsetMinVal = ImVec2(g->Style.DisplaySafeAreaPadding.x, ImMax(g->Style.DisplaySafeAreaPadding.y - g->Style.FramePadding.y, 0.0f));
			ImGui::SetNextWindowPos(ImVec2(g->IO.DisplaySize.x * 5.0f / 6.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x * 1.0f/6.0f, g->IO.DisplaySize.y * 0.75f));
			ImGui::ShowDemoWindow(&show_demo_window,&mWindowMaxSize);

		}
		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static int counter = 0;

			ImGui::SetNextWindowPos(ImVec2(0.0f, MainMenuBarHeight));
			ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x * 1.0f/6.0f, g->IO.DisplaySize.y*0.75f- MainMenuBarHeight));
			ImGui::SetNextWindowBgAlpha(1);

			ImGui::Begin("Hello, world!",&helloWindow,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);                          // Create a window called "Hello, world!" and append into it.
			
			//ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			//ImGui::Checkbox("Debug Window", &show_another_window);

			ImGui::Text("Camera Move Speed");
			ImGui::SliderFloat("   ", &CameraMoveSpeed, 0.0f, 500.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

			ImGui::Text("Show Sence");
			ImGui::Checkbox("Shadow", &show_shadow);
			ImGui::Checkbox("Model", &show_model);
			ImGui::Checkbox("Sky", &show_sky);
			ImGui::Checkbox("Land", &show_land);
			ImGui::Checkbox("HeightMapTerrain", &show_HeightMapTerrain);

			if (ImGui::CollapsingHeader("Water", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Water Rendering", &show_water);
				ImGui::SliderFloat("Transparent", &mWaterTransparent, 0.0f, 500.0f);
			}

			if (ImGui::CollapsingHeader("Grass", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Grass Rendering", &show_Grass);
				ImGui::Checkbox("RandomSize", &mIsGrassRandom);
				ImGui::ColorEdit3("Grass color", mGrassColor, 0);
				ImGui::SliderFloat("Height", &mPerGrassHeight, 3.0f, 10.0f);
				ImGui::SliderFloat("Width", &mPerGrassWidth, 0.5f, 3.0f);
				ImGui::SliderFloat("MinWind", &GrassMinWind, 0.0f, 1.0f);
				ImGui::SliderFloat("MaxWind", &GrassMaxWind, 1.0f, 2.5f);
			}

			if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Fog Rendering", &FogEnabled);
				ImGui::Text("Fog Start");
				ImGui::SliderFloat(" ", &FogStart, 0.0f, 1000.0f);
				ImGui::Text("Fog Range");
				ImGui::SliderFloat("  ", &FogRange, 0.0f, 1000.0f);
			}
				//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			

			ImGui::End();
		}

		// 3. Show Output log window.
		if (show_another_window)
		{
			//g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
			ImGui::SetNextWindowPos(ImVec2(0.0f, g->IO.DisplaySize.y * 0.75f));
			ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x, g->IO.DisplaySize.y * 0.25f));
			ImGui::SetNextWindowBgAlpha(1);

			ImGui::Begin("Debug Window", &show_another_window,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			if (ImGui::Button("Clear")) { EngineLog::ClearLogs(); }ImGui::SameLine();
			if (ImGui::Button("Copy")) { ImGui::LogToClipboard(); }ImGui::SameLine();
			ImGui::SetNextItemWidth(200);
			ImGui::InputText("Filter", FilterString, IM_ARRAYSIZE(FilterString));
			ImGui::Separator();
		
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); // 1 separator, 1 input text
			ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar); // Leave room for 1 separator + 1 InputText

			ImGui::Text("Camera Position X:%.1f Y:%.1f Z:%.1f", mCameraPosition.x, mCameraPosition.y, mCameraPosition.z);

			//Filter logs
			string Filter(FilterString);
			if (strcmp(FilterString, "") != 0)
			{
				EngineLog::Filter(StringToWString(Filter));
				mLogs = EngineLog::GetFilterLogs();
			}
			else
			{
				mLogs = EngineLog::GetLogs();
			}
			//Display logs
			for (auto& e : mLogs)
			{
				ImGui::Text((">>"+WStringToString(e)).c_str());
			}

			//将滚动条滑倒最后一条log
			int LogSize = mLogs.size();
			if (LogSize != mLogSize)
			{
				ImGui::SetScrollY(ImGui::GetFrameHeightWithSpacing()*LogSize);
				mLogSize = LogSize;
			}
			ImGui::EndChild();
			ImGui::SetNextItemWidth(300);
			ImGui::InputText("Input", InputBuffer, IM_ARRAYSIZE(InputBuffer));
			ImGui::End();
		}

		//Stat Windows
		{
			ImGui::SetNextWindowPos(ImVec2(ImVec2(g->IO.DisplaySize.x / 5.8f, MainMenuBarHeight+5.0f)));
			ImGui::SetNextWindowSize(ImVec2(120.0f, 200.0f));
			bool stat = true;
			ImGui::Begin("Stat Window", &stat, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
			if (ImGui::CollapsingHeader("  States"))
			{
				if (ImGui::RadioButton("Wireframe", mWireframe))
				{
					if (mWireframe)
					{
						mWireframe = false;
					}
					else
					{
						mWireframe = true;
					}
				}
				
			}
			ImGui::End();
		}



		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3dCommandList);
	}
}

void EngineGUI::SetWindowStyles()
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.Colors[ImGuiCol_Text] = ImVec4(0.447, 0.488, 0.488, 1);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05, 0.05, 0.05, 1);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.128, 0.061, 0.061, 1);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0, 0, 0, 1);
	style.Colors[ImGuiCol_Header] = ImVec4(0.19, 0.233, 0.284, 0.8);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.101, 0.150, 0.205, 1);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.101, 0.050, 0.075, 1);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.101, 0.150, 0.215, 0.5);
	style.Colors[ImGuiCol_Border] = ImVec4(0.1, 0.1, 0.1, 1);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.101, 0.050, 0.075, 1);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.101, 0.050, 0.075, 1);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.215, 0.347, 0.517, 1);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05, 0.07, 0.07, 1);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.05, 0.17, 0.17, 1);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.15, 0.27, 0.27, 1);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15, 0.27, 0.27, 1);
}

