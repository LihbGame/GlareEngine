#pragma once
#include <string>

class Scene;
using namespace std;
class EngineGlobal
{
public:
	static string TextureAssetPath;
	static string TerrainAssetPath;
	static string WaterAssetPath;
	static string SkyAssetPath;
	static string ModelAssetPath;

	static Scene* gCurrentScene;
};
	
