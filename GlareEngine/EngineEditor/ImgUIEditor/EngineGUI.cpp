#include "EngineGUI.h"
#include <dxgi.h>
#include "Engine/EngineUtility.h"
#include "Engine/EngineLog.h"
#include "Engine/EngineAdjust.h"
#include "Graphics/GraphicsCore.h"
#include "Engine/GameCore.h"
#include "Graphics/TextureManager.h"
#include "Graphics/Render.h"
#include "Engine/Scene.h"

using Microsoft::WRL::ComPtr;
using namespace GlareEngine::GameCore;
bool gFullSreenMode = false;
bool EngineGUI::mWindowMaxSize = false;
unordered_map<string,vector<RenderPassDebugInfo>> EngineGUI::mRenderPassDebugInfo;

DescriptorHeap EngineGUI::mGUISrvDescriptorHeap;
UINT EngineGUI::mCurrentDescriptorOffset = 0;
UINT EngineGUI::mUISystemDescriptorOffset;

EngineGUI::EngineGUI(ID3D12GraphicsCommandList* d3dCommandList)
{
	CreateUIDescriptorHeap(d3dCommandList);
	InitGUI();
}

EngineGUI::~EngineGUI()
{
	mGUISrvDescriptorHeap.Destroy();
}

XMFLOAT2 EngineGUI::GetEngineLogoSize()
{
	float IconSize = g->IO.DisplaySize.x * 0.075f;
	return XMFLOAT2(IconSize, IconSize * 1.15f);
}

void EngineGUI::SetScenes(vector<Scene*> scenes)
{
	for (auto& scene: scenes)
	{
		mSceneName += scene->GetName() + '\0';
	}
}

void EngineGUI::AddRenderPassVisualizeTexture(string FeatureGroup, string TextureName, float TextureHeight, float TextureWidth, D3D12_CPU_DESCRIPTOR_HANDLE TexDescriptor, Vector4 ColorScale)
{
	float Rate = TextureHeight / TextureWidth;

	UINT destCount = 1;
	g_Device->CopyDescriptors(1, &mGUISrvDescriptorHeap[mCurrentDescriptorOffset], &destCount,
		destCount, &TexDescriptor, &destCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	RenderPassDebugInfo renderInfo = RenderPassDebugInfo{ TextureName, Rate,TextureWidth,TextureHeight,ColorScale,CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_GPU_DESCRIPTOR_HANDLE(mGUISrvDescriptorHeap[mCurrentDescriptorOffset])) };
	mRenderPassDebugInfo[FeatureGroup].push_back(renderInfo);
	mCurrentDescriptorOffset++;
}

void EngineGUI::InitGUI()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();
	 // Setup Platform/Renderer bindings

	ID3D12DescriptorHeap* descriptorHeap = mGUISrvDescriptorHeap.GetHeapPointer();

	ImGui_ImplWin32_Init(g_hWnd);
	ImGui_ImplDX12_Init(g_Device, Render::gNumFrameResources,
		DXGI_FORMAT_R10G10B10A2_UNORM, descriptorHeap,
		descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		descriptorHeap->GetGPUDescriptorHandleForHeapStart());



	// Load Fonts
   // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
   // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
   // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
   // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
   // - Read 'docs/FONTS.txt' for more instructions and details.
   // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
   //io.Fonts->AddFontDefault();
   io.Fonts->AddFontFromFileTTF("../Resource/UImisc/fonts/Roboto-Medium.ttf", 16.0f);
   //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("UImisc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("misc/fonts/Roboto-Medium.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);

	 g = ImGui::GetCurrentContext();
	 g->Style.WindowRounding = 0;
	 g->Style.DisplaySafeAreaPadding = ImVec2(3, 5);
	 g->Style.WindowBorderSize = 1.0f;
	 SetWindowStyles();
}

void EngineGUI::CreateUIDescriptorHeap(ID3D12GraphicsCommandList* d3dCommandList)
{
	mGUISrvDescriptorHeap.Create(L"UI Texture Descriptors", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, UI_DESCRIPTOR_HEAP_SIZE);
	ID3D12DescriptorHeap* descriptorHeap = mGUISrvDescriptorHeap.GetHeapPointer();

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	UINT SRVDescriptorHandleIncrementSize=g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//the first descriptor is for the font texture
	hDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	
	auto Tex = TextureManager::GetInstance(d3dCommandList)->GetTexture(L"ICONS\\gamecontrolle", false)->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.TextureCube.MostDetailedMip = 0;
	SrvDesc.TextureCube.MipLevels = Tex->GetDesc().MipLevels;
	SrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	SrvDesc.Format = Tex->GetDesc().Format;
	g_Device->CreateShaderResourceView(Tex.Get(), &SrvDesc, hDescriptor);
	
	hDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	Tex = TextureManager::GetInstance(d3dCommandList)->GetTexture(L"ICONS\\max", false)->Resource;
	SrvDesc.TextureCube.MipLevels = Tex->GetDesc().MipLevels;
	SrvDesc.Format = Tex->GetDesc().Format;
	g_Device->CreateShaderResourceView(Tex.Get(), &SrvDesc, hDescriptor);

	hDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	Tex = TextureManager::GetInstance(d3dCommandList)->GetTexture(L"ICONS\\min", false)->Resource;
	SrvDesc.TextureCube.MipLevels = Tex->GetDesc().MipLevels;
	SrvDesc.Format = Tex->GetDesc().Format;
	g_Device->CreateShaderResourceView(Tex.Get(), &SrvDesc, hDescriptor);

	hDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	Tex = TextureManager::GetInstance(d3dCommandList)->GetTexture(L"ICONS\\close", false)->Resource;
	SrvDesc.TextureCube.MipLevels = Tex->GetDesc().MipLevels;
	SrvDesc.Format = Tex->GetDesc().Format;
	g_Device->CreateShaderResourceView(Tex.Get(), &SrvDesc, hDescriptor);

	mEngineIconTexDescriptor = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	mEngineIconTexDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	mEngineMaxTexDescriptor = mEngineIconTexDescriptor;
	mEngineMaxTexDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	mEngineMinTexDescriptor = mEngineMaxTexDescriptor;
	mEngineMinTexDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);
	mEngineCloseTexDescriptor = mEngineMinTexDescriptor;
	mEngineCloseTexDescriptor.Offset(1, SRVDescriptorHandleIncrementSize);

	mCurrentDescriptorOffset += 5;
	mUISystemDescriptorOffset = 5;
}


void EngineGUI::BeginDraw(ID3D12GraphicsCommandList* d3dCommandList)
{
	ID3D12DescriptorHeap* descriptorHeaps[] = { mGUISrvDescriptorHeap.GetHeapPointer()};
	d3dCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	// Our state

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowBgAlpha(1);

	XMFLOAT2 LogoSize = GetEngineLogoSize();
	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	if (mShowControlPanel)
	{
		DrawMainMenuBar(&mWindowMaxSize, gFullSreenMode);
		//ImGui::ShowDemoWindow(&mShowControlPanel, &mWindowMaxSize);
	}

	DrawRenderDebugWindow();

	//2.Engine Logo
    DrawEngineIcon(LogoSize.x, LogoSize.y);

	// 3. Show Output log window.
	if (mShowDebugwindow)
	{
		DrawDebugWindow();
	}

	//4. Stat Windows
	DrawStatWindow();

	//5. Control Panel
	DrawControlPanel(LogoSize.y);
}

void EngineGUI::EndDraw(ID3D12GraphicsCommandList* d3dCommandList)
{
	//end Control Panel window
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3dCommandList);
}

void EngineGUI::ShutDown()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void EngineGUI::SetWindowStyles()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_Text] = ImVec4(0.6f, 0.6f, 0.6f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.94f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4f, 0.4f, 0.4f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.5f, 0.5f, 0.5f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3f, 0.3f, 0.3f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.4f, 0.4f, 0.4f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 0.80f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.3f, 0.4f, 0.4f, 0.91f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.4f, 0.5f, 0.5f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.5f, 0.6f, 0.6f, 1.00f);
	colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.80f);
	colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
	colors[ImGuiCol_TabActive] = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
	colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
	colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}

void EngineGUI::DrawEngineIcon(float IconSize, float IconWindowHigh)
{
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x * CLIENT_FROMLEFT, IconWindowHigh));
	ImGui::SetNextWindowBgAlpha(1);
	ImGui::Begin("Engine Icon", &isUIShow, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
	ImGui::Image((void*)(mEngineIconTexDescriptor.ptr), ImVec2(g->IO.DisplaySize.x * 0.0417f, 0), ImVec2(IconSize, IconSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
	ImGui::End();
}

void EngineGUI::DrawControlPanel(float IconWindowHigh)
{
	ImGui::SetNextWindowPos(ImVec2(0.0f, IconWindowHigh));
	ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x * CLIENT_FROMLEFT, int(g->IO.DisplaySize.y * CLIENT_HEIGHT + 1.0f) - IconWindowHigh));
	ImGui::SetNextWindowBgAlpha(1);

	ImGui::Begin("Control Panel", &isUIShow, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

	ImGui::Text("Camera Move Speed");
	ImGui::SliderFloat("   ", &CameraMoveSpeed, 0.0f, 10.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

	ImGui::Separator();
	ImGui::Text("Render Pipeline");
	ImGui::Combo(" ", &mRenderPipelineIndex, "TBFR\0TBDR\0");


	ImGui::Separator();
	ImGui::Text("Choose Scene");
	ImGui::Combo("", &mSceneIndex, mSceneName.c_str());

	if (ImGui::CollapsingHeader("Scene Setting", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Shadow", &show_shadow);
		ImGui::Checkbox("Model", &show_model);
		ImGui::Checkbox("Sky", &show_sky);
		ImGui::Checkbox("Terrain", &show_HeightMapTerrain);
	}

	//if (ImGui::CollapsingHeader("Water", ImGuiTreeNodeFlags_DefaultOpen))
	//{
	//	ImGui::Checkbox("Water Rendering", &show_water);
	//	ImGui::SliderFloat("Transparent", &mWaterTransparent, 0.0f, 500.0f);
	//}

	//if (ImGui::CollapsingHeader("Grass", ImGuiTreeNodeFlags_DefaultOpen))
	//{
	//	ImGui::Checkbox("Grass Rendering", &show_Grass);
	//	ImGui::Checkbox("RandomSize", &mIsGrassRandom);
	//	ImGui::ColorEdit3("Grass color", mGrassColor, 0);
	//	ImGui::SliderFloat("Height", &mPerGrassHeight, 3.0f, 10.0f);
	//	ImGui::SliderFloat("Width", &mPerGrassWidth, 0.5f, 3.0f);
	//	ImGui::SliderFloat("MinWind", &GrassMinWind, 0.0f, 1.0f);
	//	ImGui::SliderFloat("MaxWind", &GrassMaxWind, 1.0f, 2.5f);
	//}

	//if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen))
	//{
	//	ImGui::Checkbox("Fog Rendering", &FogEnabled);
	//	ImGui::Text("Fog Start");
	//	ImGui::SliderFloat(" ", &FogStart, 0.0f, 1000.0f);
	//	ImGui::Text("Fog Range");
	//	ImGui::SliderFloat("  ", &FogRange, 0.0f, 1000.0f);
	//}
}

void EngineGUI::DrawDebugWindow()
{
	//g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
	ImGui::SetNextWindowPos(ImVec2(0.0f, g->IO.DisplaySize.y * 0.75f));
	ImGui::SetNextWindowSize(ImVec2(g->IO.DisplaySize.x, ceil(g->IO.DisplaySize.y * 0.25f)));
	ImGui::SetNextWindowBgAlpha(1);

	ImGui::Begin("Debug Window", &mShowDebugwindow, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
	if (ImGui::Button("Clear")) { EngineLog::ClearLogs(); }ImGui::SameLine();
	if (ImGui::Button("Copy")) { ImGui::LogToClipboard(); }ImGui::SameLine();
	ImGui::SetNextItemWidth(200);
	ImGui::InputText("Filter", FilterString, IM_ARRAYSIZE(FilterString)); ImGui::SameLine();
	ImGui::Text("    Frame Time: %.3f ms  FPS: %.1f", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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
		ImGui::Text((">>" + WStringToString(e)).c_str());
	}

	//将滚动条滑倒最后一条log
	int LogSize = (int)mLogs.size();
	if (LogSize != mLogSize)
	{
		ImGui::SetScrollY(ImGui::GetFrameHeightWithSpacing() * LogSize);
		mLogSize = LogSize;
	}
	ImGui::EndChild();
	ImGui::SetNextItemWidth(300);
	ImGui::InputText("Input", InputBuffer, IM_ARRAYSIZE(InputBuffer));
	ImGui::End();
}

void EngineGUI::DrawStatWindow()
{
	ImGui::SetNextWindowPos(ImVec2(ImVec2(g->IO.DisplaySize.x / 5.8f, MainMenuBarHeight + 5.0f)));
	ImGui::SetNextWindowSize(ImVec2(120.0f, 200.0f));
	bool stat = true;

	ImGui::Begin("Stat Window", &stat, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
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

void EngineGUI::DrawRenderDebugWindow()
{
	//Frame Rounding
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.FrameRounding = 7;
		style.GrabRounding = 7;
		//style.WindowRounding = 7;
		//style.WindowBorderSize = 1.5f;
	}

	// Demonstrate the various window flags. Typically you would just use the default!
	static bool no_titlebar = true;
	static bool no_scrollbar = false;
	static bool no_menu = true;
	static bool no_move = true;
	static bool no_resize = true;
	static bool no_collapse = false;
	static bool no_close = false;
	static bool no_nav = false;
	static bool no_background = false;
	static bool no_bring_to_front = false;

	ImGuiWindowFlags window_flags = 0;
	if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
	if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
	if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
	if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
	if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
	if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
	if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
	if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
	if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 5.0f / 6.0f, MainMenuBarHeight));
	ImVec2 windowSize = ImVec2(ImGui::GetIO().DisplaySize.x * CLIENT_FROMLEFT + 1.0f, ImGui::GetIO().DisplaySize.y * CLIENT_HEIGHT - MainMenuBarHeight);
	ImGui::SetNextWindowSize(windowSize);
	ImGui::SetNextWindowBgAlpha(1);

	if (!ImGui::Begin("Render Debug Window", NULL, window_flags))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	int textureWidth = windowSize.x - 30;
	g = ImGui::GetCurrentContext();
	g->Style.Alpha = 1.0f;
	for (auto& Feature : mRenderPassDebugInfo)
	{
		if (ImGui::CollapsingHeader(Feature.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (auto& FeatureTexture : Feature.second)
			{
				ImGui::Text(FeatureTexture.TextureName.c_str()); ImGui::SameLine();
				ImGui::Text("%0.0f X %0.0f", FeatureTexture.TextureWidth, FeatureTexture.TextureHeight);
				int textureHeight = textureWidth * FeatureTexture.Rate;
				ImVec4 ColorScale = ImVec4(FeatureTexture.TextureColorScale.GetX(), FeatureTexture.TextureColorScale.GetY(),
					FeatureTexture.TextureColorScale.GetZ(), FeatureTexture.TextureColorScale.GetW());
				ImGui::Image((void*)(FeatureTexture.TexDescriptor.ptr), ImVec2(0, 0), ImVec2(textureWidth, textureHeight), ImVec2(0, 0), ImVec2(1, 1), ColorScale);
				ImGui::Separator();
			}
		}
	}
	ImGui::End();
	return;
}

void EngineGUI::DrawMainMenuBar(bool* IsMax, bool IsFullScreenMode)
{
	if (ImGui::BeginMainMenuBar())
	{
		//if (ImGui::BeginMenu("File"))
		//{
		//	ImGui::EndMenu();
		//}
		//if (ImGui::BeginMenu("Edit"))
		//{
		//	if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
		//	if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
		//	ImGui::Separator();
		//	if (ImGui::MenuItem("Cut", "CTRL+X")) {}
		//	if (ImGui::MenuItem("Copy", "CTRL+C")) {}
		//	if (ImGui::MenuItem("Paste", "CTRL+V")) {}
		//	ImGui::EndMenu();
		//}
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 pos = ImGui::GetWindowSize();
		if (!IsFullScreenMode)
		{
			ImGui::GetStyle().Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 0.0f);
			if (ImGui::ImageButton((void*)(mEngineMinTexDescriptor.ptr), ImVec2(pos.x - 170.0f, -7), ImVec2(33, 33), ImVec2(0, 0), ImVec2(1, 1),5))
			{
				SendMessage((HWND)io.ImeWindowHandle, WM_SYSCOMMAND, SC_MINIMIZE, NULL);
			}
			if (ImGui::ImageButton((void*)(mEngineMaxTexDescriptor.ptr), ImVec2(pos.x - 160.0f, -7), ImVec2(33, 33), ImVec2(0, 0), ImVec2(1, 1),5))
			{
				*IsMax = !(*IsMax);
			}
			if (ImGui::ImageButton((void*)(mEngineCloseTexDescriptor.ptr), ImVec2(pos.x - 150.0f, -7), ImVec2(33, 33), ImVec2(0, 0), ImVec2(1, 1),5))
			{
				SendMessage((HWND)io.ImeWindowHandle, WM_CLOSE, NULL, NULL);
			}
			ImGui::GetStyle().Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 0.4f);
		}
		else
		{
			if (ImGui::RadioButton("Close", true, ImVec2(ImGui::GetWindowSize().x - 160.0f, 0.0f)))
			{
				SendMessage((HWND)io.ImeWindowHandle, WM_CLOSE, NULL, NULL);
			}
		}
		ImGui::EndMainMenuBar();
		ImGui::GetStyle().FramePadding.y = 3;
	}
}