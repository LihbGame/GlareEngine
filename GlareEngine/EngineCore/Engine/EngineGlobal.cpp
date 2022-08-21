#include "EngineGlobal.h"

std::wstring EngineGlobal::TextureAssetPath = L"../Resource/Textures/";
std::wstring EngineGlobal::TerrainAssetPath = TextureAssetPath + L"Terrain/";
std::wstring EngineGlobal::WaterAssetPath = TextureAssetPath + L"Water/";
std::wstring EngineGlobal::SkyAssetPath = TextureAssetPath + L"HDRSky/";
std::wstring EngineGlobal::ModelAssetPath = L"../Resource/Model/";

Scene* EngineGlobal::gCurrentScene = nullptr;
