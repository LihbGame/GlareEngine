#pragma once
#include "L3DUtil.h"

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


class L3DVertice
{
};

