#pragma once
#include "EngineUtility.h"

namespace Vertices
{
	struct Pos
	{
		 Pos() :Position(0, 0, 0) {}
		 Pos(const DirectX::XMFLOAT3& p) :Position(p) {}
		 Pos(float px, float py, float pz) :Position(px, py, pz) {}
		
		 DirectX::XMFLOAT3 Position;
	};

	struct PosNormalTexc
	{
		PosNormalTexc() :
			Position(0, 0, 0),
			Normal(0, 0, 0),
			Texc(0, 0) {}
		PosNormalTexc(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			Texc(uv) {}
		PosNormalTexc(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			Texc(u, v) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 Texc;
	};

	struct PosNormalTangentTexc
	{
		PosNormalTangentTexc() :
			Position(0, 0, 0),
			Normal(0, 0, 0),
			Tangent(0, 0, 0),
			Texc(0, 0) {}
		PosNormalTangentTexc(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT3& t,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			Tangent(t),
			Texc(uv) {}
		PosNormalTangentTexc(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			Tangent(tx, ty, tz),
			Texc(u, v) {}


		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 Tangent;
		DirectX::XMFLOAT2 Texc;
	};


	struct Terrain
	{
		Terrain() :
			Position(0, 0, 0),
			Tex(0, 0),
			BoundsY(0, 0) {}
		Terrain(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT2& n,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Tex(n),
			BoundsY(uv) {}
		Terrain(
			float px, float py, float pz,
			float nx, float ny,
			float u, float v) :
			Position(px, py, pz),
			Tex(nx, ny),
			BoundsY(u, v) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 Tex;
		DirectX::XMFLOAT2 BoundsY;
	};


	struct Grass
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 Tex;
	};

	struct Particle
	{
		XMFLOAT3 InitialPos;
		XMFLOAT3 InitialVel;
		XMFLOAT2 Size;
		float Age;
		unsigned int Type;
	};

};