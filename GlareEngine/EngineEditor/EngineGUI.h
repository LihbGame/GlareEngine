#pragma once
//GUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <imgui_internal.h>
#include "L3DUtil.h"

#define MainMenuBarHeight  19.0f

class EngineGUI
{
public:
	EngineGUI(ID3D12Device* d3dDevice);
	~EngineGUI();

	void InitGUI(HWND GameWnd,ID3D12DescriptorHeap* GUISrvDescriptorHeap);
	void CreateUIDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& DescriptorHeap);
	void DrawUI(ID3D12GraphicsCommandList* d3dCommandList);


	bool IsShowShadow() { return show_shadow; }
	bool IsShowModel() { return show_model; }
	bool IsShowWater() { return show_water; }
	bool IsShowLand() { return show_land; }
	bool IsShowSky() { return show_sky; }
	bool IsShowFog() { return FogEnabled; }
	bool IsShowTerrain() { return show_HeightMapTerrain; }
	float GetCameraModeSpeed() { return CameraMoveSpeed; }
	float GetFogStart() { return FogStart; }
	float GetFogRange() { return FogRange; }
	bool IsShowGrass() { return show_Grass; }
private:
	bool show_demo_window = true;
	bool show_another_window = true;

	bool show_shadow = false;
	bool show_model = false;
	bool show_water = true;
	bool show_land = false;
	bool show_sky = true;
	bool show_HeightMapTerrain = true;
	bool show_Grass = true;
	float CameraMoveSpeed = 50.0f;
	bool  FogEnabled = false;
	float FogStart = 300.0f;
	float FogRange = 600.0f;

	ID3D12DescriptorHeap* mGUISrvDescriptorHeap = nullptr;

	ImGuiContext *g=nullptr;
	bool helloWindow = true;
	ID3D12Device* pd3dDevice;
};

