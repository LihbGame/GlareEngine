#include "ParticleEffect.h"

ParticleEffect::ParticleEffect(ParticleEffectProperties& effectProperties)
{
	m_ElapsedTime = 0.0;
	m_EffectProperties = effectProperties;
}

void ParticleEffect::LoadDeviceResources(ID3D12Device* device)
{
}

void ParticleEffect::Update(ComputeContext& CompContext, float timeDelta)
{
}

void ParticleEffect::Reset()
{
}
