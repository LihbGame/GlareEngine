#pragma once

#include "Engine/EngineUtility.h"

__declspec(align(16)) struct EmissionProperties
{
	XMFLOAT3		LastEmitPosW;
	float			EmitSpeed;
	XMFLOAT3		EmitPosW;
	float			FloorHeight;
	XMFLOAT3		EmitDirW;
	float			Restitution;
	XMFLOAT3		EmitRightW;
	float			EmitterVelocitySensitivity;
	XMFLOAT3		EmitUpW;
	UINT			MaxParticles;
	XMFLOAT3		Gravity;
	UINT			TextureID;
	XMFLOAT3		EmissiveColor;
	float			pad1;
	XMUINT4			RandIndex[64];
};


struct ParticleSpawnData
{
	float AgeRate;
	float RotationSpeed;
	float StartSize;
	float EndSize;
	XMFLOAT3 Velocity; float Mass;
	XMFLOAT3 SpreadOffset; float Random;
	Color StartColor;
	Color EndColor;
};


struct ParticleMotion
{
	XMFLOAT3 Position;
	float Mass;
	XMFLOAT3 Velocity;
	float Age;
	float Rotation;
	UINT ResetDataIndex;
};

struct ParticleVertex
{
	XMFLOAT3 Position;
	XMFLOAT4 Color;
	float Size;
	UINT TextureID;
};

struct ParticleScreenData
{
	float Corner[2];
	float RcpSize[2];
	float Color[4];
	float Depth;
	float TextureIndex;
	float TextureLevel;
	uint32_t Bounds;
};

EmissionProperties* CreateEmissionProperties();