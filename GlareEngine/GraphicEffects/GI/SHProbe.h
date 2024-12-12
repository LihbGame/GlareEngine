#pragma once
#include "Misc/CubeRenderTarget.h"
#include "Graphics/DepthBuffer.h"
#include "Animation/ModelMesh.h"

#define PROBE_TARGET_RESOLUTION 512

class SHProbe
{
public:
	~SHProbe();

	SHProbe(XMFLOAT3 position, CubeRenderTarget& cubeTarget);

	void PreComputeSH();

	void PrepareCubeMap(Scene& scene, GraphicsContext& Context);

	void StoreSH();
	 
	void SetPosition(XMFLOAT3 probePosition) { mProbePosition = probePosition; }

	XMFLOAT3 GetPosition() const { return mProbePosition; }
private:
	XMFLOAT3 mProbePosition;
	CubeRenderTarget& mCubeTarget;
};

