#pragma once
#include <xstring>

class Scene;

class EngineGlobal
{
public:
	static std::wstring TextureAssetPath;
	static std::wstring TerrainAssetPath;
	static std::wstring WaterAssetPath;
	static std::wstring SkyAssetPath;
	static std::wstring ModelAssetPath;

	static Scene* gCurrentScene;
};
	
