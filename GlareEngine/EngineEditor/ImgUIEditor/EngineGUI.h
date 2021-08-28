#pragma once
//GUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <imgui_internal.h>
#include "EngineUtility.h"


#define MainMenuBarHeight  27.0f
#define CLIENT_FROMLEFT 0.166667f
#define CLIENT_HEIGHT 0.75f

extern bool gFullSreenMode;
class EngineGUI
{
public:
	EngineGUI(ID3D12GraphicsCommandList* d3dCommandList);
	~EngineGUI();
public:
	void Draw(ID3D12GraphicsCommandList* d3dCommandList);
	void ShutDown();

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

	bool IsWireframe()const { return mWireframe; }

	static bool mWindowMaxSize;
private:
	void InitGUI();
	void CreateUIDescriptorHeap(ID3D12GraphicsCommandList* d3dCommandList);
	void SetWindowStyles();
private:

	bool show_demo_window = true;
	bool show_another_window = true;

	bool show_shadow = false;
	bool show_model = false;
	bool show_water = false;
	bool show_land = false;
	bool show_sky = true;
	bool show_HeightMapTerrain = true;
	bool show_Grass = false;
	float CameraMoveSpeed = 200.0f;

	bool  FogEnabled = false;
	float FogStart = 300.0f;
	float FogRange = 600.0f;

	float GrassMinWind = 0.5f;
	float GrassMaxWind = 2.5f;

	float mPerGrassHeight = 10.0f;
	float mPerGrassWidth = 0.5f;
	bool mIsGrassRandom = true;
	float mWaterTransparent = 100.0f;

	bool mWireframe = 0;

	float mGrassColor[3] = { 0.39f,0.196f,0.0f };
	XMFLOAT3 mCameraPosition = { 0.0f,0.0f,0.0f };

	vector<wstring> mLogs;
	char  FilterString[256] = {};
	char InputBuffer[256] = {};
	int mLogSize = 0;
private:
	ID3D12DescriptorHeap* mGUISrvDescriptorHeap = nullptr;
	ImGuiContext *g=nullptr;
	bool isUIShow = true;
};

