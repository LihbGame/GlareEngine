#pragma once
#include "EngineUtility.h"
class InputLayout
{
public:
	static std::vector<D3D12_INPUT_ELEMENT_DESC> Pos;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> PosNormalTexc;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> PosNormalTangentTexc;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> InstancePosNormalTangentTexc;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> SkinAnimeVertex;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> HeightMapTerrain;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> Grass;
	static std::vector<D3D12_INPUT_ELEMENT_DESC> Particle;
};

