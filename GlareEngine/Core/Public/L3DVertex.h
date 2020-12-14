#pragma once
#include "L3DUtil.h"

class L3DVertice
{
public:
	 struct Pos
	{
		DirectX::XMFLOAT3 Pos;
	};

	struct PosNormalTexc
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 Texc;
	};

	struct PosNormalTangentTexc
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT2 Texc;
	};


	struct Terrain
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT2 Tex;
		DirectX::XMFLOAT2 BoundsY;
	};


	struct Grass
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT2 BoundsY;
	};

};