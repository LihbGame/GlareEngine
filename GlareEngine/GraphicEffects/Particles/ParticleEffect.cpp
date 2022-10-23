#include "ParticleEffect.h"
#include "Math/Random.h"

using namespace GlareEngine::Math;

inline static Color RandColor(Color c0, Color c1)
{
	return Color(
		g_Random.NextFloat(c0.R(), c1.R()),
		g_Random.NextFloat(c0.G(), c1.G()),
		g_Random.NextFloat(c0.B(), c1.B()),
		g_Random.NextFloat(c0.A(), c1.A())
	);
}

inline static XMFLOAT3 RandSpread(const XMFLOAT3& s)
{
	return XMFLOAT3(
		g_Random.NextFloat(-s.x, s.x),
		g_Random.NextFloat(-s.y, s.y),
		g_Random.NextFloat(-s.z, s.z)
	);
}



ParticleEffect::ParticleEffect(ParticleEffectProperties& effectProperties)
{
	m_ElapsedTime = 0.0;
	m_EffectProperties = effectProperties;
}

void ParticleEffect::InitializeResources(ID3D12Device* device)
{
	//use to reset
	m_OriginalEffectProperties = m_EffectProperties; 

	//Fill particle spawn data buffer
	ParticleSpawnData* pSpawnData = (ParticleSpawnData*)_malloca(m_EffectProperties.EmitProperties.MaxParticles * sizeof(ParticleSpawnData));

	for (UINT i = 0; i < m_EffectProperties.EmitProperties.MaxParticles; i++)
	{
		ParticleSpawnData& SpawnData = pSpawnData[i];

		SpawnData.AgeRate = 1.0f / g_Random.NextFloat(m_EffectProperties.LifeMinMax.x, m_EffectProperties.LifeMinMax.y);
		
		float horizontalAngle = g_Random.NextFloat(XM_2PI);
		float horizontalVelocity = g_Random.NextFloat(m_EffectProperties.Velocity.GetX(), m_EffectProperties.Velocity.GetY());
		SpawnData.Velocity.x = horizontalVelocity * cos(horizontalAngle);
		SpawnData.Velocity.y = g_Random.NextFloat(m_EffectProperties.Velocity.GetZ(), m_EffectProperties.Velocity.GetW());
		SpawnData.Velocity.z = horizontalVelocity * sin(horizontalAngle);

		SpawnData.SpreadOffset = RandSpread(m_EffectProperties.Spread);

		SpawnData.StartSize = g_Random.NextFloat(m_EffectProperties.Size.GetX(), m_EffectProperties.Size.GetY());
		SpawnData.EndSize = g_Random.NextFloat(m_EffectProperties.Size.GetZ(), m_EffectProperties.Size.GetW());
		
		SpawnData.StartColor = RandColor(m_EffectProperties.MinStartColor, m_EffectProperties.MaxStartColor);
		SpawnData.EndColor = RandColor(m_EffectProperties.MinEndColor, m_EffectProperties.MaxEndColor);
		
		SpawnData.Mass = g_Random.NextFloat(m_EffectProperties.MassMinMax.x, m_EffectProperties.MassMinMax.y);
		SpawnData.RotationSpeed = g_Random.NextFloat();
		SpawnData.Random = g_Random.NextFloat();
	}

	m_RandomStateBuffer.Create(L"ParticleSystem::SpawnDataBuffer", m_EffectProperties.EmitProperties.MaxParticles, sizeof(ParticleSpawnData), pSpawnData);
	_freea(pSpawnData);

	m_StateBuffers[0].Create(L"ParticleSystem::Buffer0", m_EffectProperties.EmitProperties.MaxParticles, sizeof(ParticleMotion));
	m_StateBuffers[1].Create(L"ParticleSystem::Buffer1", m_EffectProperties.EmitProperties.MaxParticles, sizeof(ParticleMotion));
	m_CurrentStateBuffer = 0;

	//DispatchIndirect args buffer / number of thread groups
	__declspec(align(16)) UINT DispatchIndirectData[3] = { 0, 1, 1 };
	m_DispatchIndirectArgs.Create(L"ParticleSystem::DispatchIndirectArgs", 1, sizeof(D3D12_DISPATCH_ARGUMENTS), DispatchIndirectData);
}

void ParticleEffect::Update(ComputeContext& CompContext, float timeDelta)
{

}

void ParticleEffect::Reset()
{
	m_EffectProperties = m_OriginalEffectProperties;
}
