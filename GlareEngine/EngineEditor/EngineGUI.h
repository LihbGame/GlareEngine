#pragma once
//GUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <imgui_internal.h>
#include <L3DUtil.h>

#define MainMenuBarHeight  22.0f

class EngineGUI
{
public:
	EngineGUI();
	~EngineGUI();

	void InitGUI(HWND GameWnd,ID3D12Device* d3dDevice, ID3D12DescriptorHeap* GUISrvDescriptorHeap);
	void DrawUI(ID3D12GraphicsCommandList* d3dCommandList);
private:
	bool show_demo_window = true;
	bool show_another_window = true;

	ID3D12DescriptorHeap* mGUISrvDescriptorHeap = nullptr;

	ImGuiContext *g=nullptr;
	bool helloWindow = true;
	
};

