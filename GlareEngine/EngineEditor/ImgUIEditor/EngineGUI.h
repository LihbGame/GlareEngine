#pragma once
//GUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <imgui_internal.h>
#include "L3DUtil.h"

#define MainMenuBarHeight  25.0f

extern bool gFullSreenMode;
class EngineGUI
{
public:
	EngineGUI(ID3D12Device* d3dDevice);
	~EngineGUI();

	void InitGUI(HWND GameWnd,ID3D12DescriptorHeap* GUISrvDescriptorHeap);
	void CreateUIDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& DescriptorHeap);
	void DrawUI(ID3D12GraphicsCommandList* d3dCommandList);


	bool IsShowShadow()const { return show_shadow; }
	bool IsShowModel()const { return show_model; }
	bool IsShowWater()const { return show_water; }
	bool IsShowLand() const { return show_land; }
	bool IsShowSky()const { return show_sky; }
	bool IsShowFog()const { return FogEnabled; }
	bool IsShowTerrain() const { return show_HeightMapTerrain; }
	float GetCameraModeSpeed()const { return CameraMoveSpeed; }
	float GetFogStart()const { return FogStart; }
	float GetFogRange()const { return FogRange; }
	bool IsShowGrass()const { return show_Grass; }

	float GetGrassMinWind()const { return GrassMinWind; }
	float GetGrassMaxWind()const { return GrassMaxWind; }

	void SetCameraPosition(const XMFLOAT3& position) { mCameraPosition = position; }

	XMFLOAT3 GetGrassColor()const { return XMFLOAT3(mGrassColor); }
	float GetPerGrassHeight()const { return mPerGrassHeight; }
	float GetPerGrassWidth()const { return mPerGrassWidth; }
	bool IsGrassRandom()const { return mIsGrassRandom; }
	float GetWaterTransparent()const { return mWaterTransparent; }

	static bool mWindowMaxSize;
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
	float CameraMoveSpeed = 200.0f;

	bool  FogEnabled = false;
	float FogStart = 300.0f;
	float FogRange = 600.0f;

	float GrassMinWind = 0.5f;
	float GrassMaxWind = 1.0f;

	float mPerGrassHeight = 5.0f;
	float mPerGrassWidth = 2.0f;
	bool mIsGrassRandom = false;
	float mWaterTransparent = 100.0f;

	float mGrassColor[3] = { 0.39f,0.196f,0.0f };
	XMFLOAT3 mCameraPosition = { 0.0f,0.0f,0.0f };

	ID3D12DescriptorHeap* mGUISrvDescriptorHeap = nullptr;

	ImGuiContext *g=nullptr;
	bool helloWindow = true;
	ID3D12Device* pd3dDevice;
};

