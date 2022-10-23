#pragma once
#include "Engine/EngineUtility.h"
class BaseParticle;
class ParticleGroup
{
public:
	ParticleGroup();
	~ParticleGroup();

	void AddParticle(BaseParticle* Particle);
	void RemoveParticle(int ParticleID);
	void DrawParticle();
private:
	vector<BaseParticle*> Particles;
	int ParticleGroupID = 0;
};


