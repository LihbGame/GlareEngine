#pragma once
#include "Graphics/GPUBuffer.h"
#include "ParticleEffectProperties.h"
#include "ParticleShaderStructs.h"

class ParticleEffect
{
public:
	ParticleEffect(ParticleEffectProperties& effectProperties);
	
	void InitializeResources(ID3D12Device* device);
	
	void Update(ComputeContext& CompContext, float timeDelta);
	
	float GetLifetime() { return m_EffectProperties.TotalActiveLifetime; }
	
	float GetElapsedTime() { return m_ElapsedTime; }
	
	void Reset();

private:

	StructuredBuffer m_StateBuffers[2];
	uint32_t m_CurrentStateBuffer;
	StructuredBuffer m_RandomStateBuffer;

	IndirectArgsBuffer m_DispatchIndirectArgs;
	IndirectArgsBuffer m_DrawIndirectArgs;

	ParticleEffectProperties m_EffectProperties;
	ParticleEffectProperties m_OriginalEffectProperties;


	float m_ElapsedTime;
	UINT m_effectID;
};
