#pragma once
#include <string>

class Scene;

class EngineGlobal
{
public:
	static std::string TextureAssetPath;
	static std::string TerrainAssetPath;
	static std::string WaterAssetPath;
	static std::string SkyAssetPath;
	static std::string ModelAssetPath;

	static Scene* gCurrentScene;
};
	
