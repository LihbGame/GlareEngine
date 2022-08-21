#pragma once

#include "EngineUtility.h"

class PSO;

class BaseParticle
{
public:
	BaseParticle();
	~BaseParticle();

	// Time elapsed since the system was reset.
	 float GetAge()const;

	 void SetEmitPos(const XMFLOAT3& emitPosW);
	 void SetEmitDir(const XMFLOAT3& emitDirW);

	 void Init(int ParticleID, std::string ParticleTextureName,
		UINT maxParticles,
		 PSO* Pso);

	 void Reset();
	 void Update(float dt, float gameTime);
	 void Draw();

private:
	void BuildVB(ID3D12Device* device);

private:
	UINT mMaxParticles;
	bool mFirstRun;

	int mParticleID;

	float mGameTime;
	float mTimeStep;
	float mAge;

	XMFLOAT3 mEmitPosW;
	XMFLOAT3 mEmitDirW;

	Microsoft::WRL::ComPtr<ID3D12Resource> mInitVB;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDrawVB;
	Microsoft::WRL::ComPtr<ID3D12Resource> mStreamOutVB;
};




