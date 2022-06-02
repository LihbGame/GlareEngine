#include "EngineGlobal.h"

string EngineGlobal::TextureAssetPath = "../Resource/Textures/";
string EngineGlobal::TerrainAssetPath = TextureAssetPath + "Terrain/";
string EngineGlobal::WaterAssetPath = TextureAssetPath + "Water/";
string EngineGlobal::SkyAssetPath = TextureAssetPath + "HDRSky/";
string EngineGlobal::ModelAssetPath = "../Resource/Model/";

Scene* EngineGlobal::gCurrentScene = nullptr;
