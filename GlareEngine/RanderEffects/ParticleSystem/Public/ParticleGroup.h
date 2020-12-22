#pragma once
#include "L3DUtil.h"
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


