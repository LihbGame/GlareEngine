#include "EngineGlobal.h"

std::string EngineGlobal::TextureAssetPath = "../Resource/Textures/";
std::string EngineGlobal::TerrainAssetPath = TextureAssetPath + "Terrain/";
std::string EngineGlobal::WaterAssetPath = TextureAssetPath + "Water/";
std::string EngineGlobal::SkyAssetPath = TextureAssetPath + "HDRSky/";
std::string EngineGlobal::ModelAssetPath = "../Resource/Model/";

Scene* EngineGlobal::gCurrentScene = nullptr;
