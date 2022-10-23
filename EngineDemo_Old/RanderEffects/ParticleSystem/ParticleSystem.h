#pragma once
#include "framework.h"
#include "Engine/EngineUtility.h"
class ParticleGroup;
class ParticleSystem
{
	ParticleSystem();

	~ParticleSystem();

	void DrawParticle();
	void AddParticleGroup(ParticleGroup* Particle);
	void RemoveParticleGroup(int ParticleGroupID);
private:

	ParticleSystem(const ParticleSystem& rhs);
	ParticleSystem& operator=(const ParticleSystem& rhs);

private:
};

