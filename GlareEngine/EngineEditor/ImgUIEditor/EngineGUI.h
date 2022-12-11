#pragma once
//GUI
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <imgui_internal.h>
#include "Engine/EngineUtility.h"


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
	void BeginDraw(ID3D12GraphicsCommandList* d3dCommandList);
	void EndDraw(ID3D12GraphicsCommandList* d3dCommandList);
	void ShutDown();

	bool IsShowShadow()const { return show_shadow; }
	bool IsShowModel()const { return show_model; }
	bool IsShowWater()const { return show_water; }
	bool IsShowSky()const { return show_sky; }
	bool IsShowFog()const { return FogEnabled; }
	bool IsShowTerrain() const { return show_HeightMapTerrain; }
	float GetCameraModeSpeed()const { return CameraMoveSpeed; }
	float GetFogStart()const { return FogStart; }
	float GetFogRange()const { return FogRange; }
	bool IsShowGrass()const { return show_Grass; }

	float GetGrassMinWind()const { return GrassMinWind; }
	float GetGrassMaxWind()const { return GrassMaxWind; }

	XMFLOAT2 GetEngineLogoSize();

	void SetCameraPosition(const XMFLOAT3& position) { mCameraPosition = position; }

	XMFLOAT3 GetGrassColor()const { return XMFLOAT3(mGrassColor); }
	float GetPerGrassHeight()const { return mPerGrassHeight; }
	float GetPerGrassWidth()const { return mPerGrassWidth; }
	bool IsGrassRandom()const { return mIsGrassRandom; }
	float GetWaterTransparent()const { return mWaterTransparent; }

	bool IsWireframe()const { return mWireframe; }
	bool IsMSAA()const { return mMSAA; }

	static bool mWindowMaxSize;
private:
	void InitGUI();
	void CreateUIDescriptorHeap(ID3D12GraphicsCommandList* d3dCommandList);
	void SetWindowStyles();
	void DrawEngineIcon(float IconSize, float IconWindowHigh);
	void DrawControlPanel(float IconWindowHigh);
	void DrawDebugWindow();
	void DrawStatWindow();
	void DrawMainMenuBar(bool* IsMax, bool IsFullScreenMode);
private:

	bool mShowControlPanel = true;
	bool mShowDebugwindow = true;

	bool show_shadow = true;
	bool show_model = true;
	bool show_water = false;

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

	bool mWireframe = false;
	bool mMSAA = true;

	float mGrassColor[3] = { 0.39f,0.196f,0.0f };
	XMFLOAT3 mCameraPosition = { 0.0f,0.0f,0.0f };

	vector<wstring> mLogs;
	char  FilterString[256] = {};
	char InputBuffer[256] = {};
	int mLogSize = 0;
private:
	ID3D12DescriptorHeap* mGUISrvDescriptorHeap = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEngineIconTexDescriptor;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEngineMaxTexDescriptor;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEngineMinTexDescriptor;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEngineCloseTexDescriptor;
	ImGuiContext *g=nullptr;
	bool isUIShow = true;
};

