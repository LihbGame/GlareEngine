#pragma once
#include "ParticleShaderStructs.h"
#include "Color.h"
#include "Vector.h"
using namespace GlareEngine::Math;

struct ParticleEffectProperties
{
	ParticleEffectProperties()
	{
		ZeroMemory(this, sizeof(*this));
	};

	Color MinStartColor;
	Color MaxStartColor;

	Color MinEndColor;
	Color MaxEndColor;

	EmissionProperties  EmitProperties;
	float EmitRate;
	XMFLOAT2 LifeMinMax;
	XMFLOAT2 MassMinMax;
	Vector4 Size;
	XMFLOAT3 Spread;
	std::wstring TexturePath;
	float TotalActiveLifetime;
	Vector4 Velocity;
};