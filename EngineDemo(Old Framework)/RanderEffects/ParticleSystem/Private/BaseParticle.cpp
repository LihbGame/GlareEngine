#include "BaseParticle.h"
#include "PSOManager.h"
#include "Vertex.h"
BaseParticle::BaseParticle()
{
	//≥ı ºªØ
	mFirstRun = true;
	mGameTime = 0.0f;
	mTimeStep = 0.0f;
	mAge = 0.0f;

	mEmitPosW = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mEmitDirW = XMFLOAT3(0.0f, 1.0f, 0.0f);
}

BaseParticle::~BaseParticle()
{
}

float BaseParticle::GetAge() const
{
	return mAge;
}

void BaseParticle::SetEmitPos(const XMFLOAT3& emitPosW)
{
	mEmitPosW = emitPosW;
}

void BaseParticle::SetEmitDir(const XMFLOAT3& emitDirW)
{
	mEmitDirW = emitDirW;
}

void BaseParticle::Init(int ParticleID, string ParticleTextureName, UINT maxParticles, PSO* Pso)
{
	mMaxParticles = maxParticles;

}

void BaseParticle::Reset()
{
	mFirstRun = true;
	mAge = 0.0f;
}

void BaseParticle::Update(float dt, float gameTime)
{
	mGameTime = gameTime;
	mTimeStep = dt;

	mAge += dt;
}

void BaseParticle::Draw()
{
}

void BaseParticle::BuildVB(ID3D12Device* device)
{
	// The initial particle emitter has type 0 and age 0.  The rest
	// of the particle attributes do not apply to an emitter.
	Vertices::Particle p;
	ZeroMemory(&p, sizeof(Vertices::Particle));
	p.Age = 0.0f;
	p.Type = 0;
}
