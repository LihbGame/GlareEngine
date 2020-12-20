#include "ParticleSystem.h"

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
}

float ParticleSystem::GetAge() const
{
	return 0.0f;
}

void ParticleSystem::SetEmitPos(const XMFLOAT3& emitPosW)
{
}

void ParticleSystem::SetEmitDir(const XMFLOAT3& emitDirW)
{
}

void ParticleSystem::Reset()
{
}

void ParticleSystem::Update(float dt, float gameTime)
{
}

void ParticleSystem::BuildVB(ID3D11Device* device)
{
}
