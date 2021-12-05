#pragma once
#include "CubeRenderTarget.h"
#include "RenderObject.h"

// Preview:
// 1.Baking Environment Diffuse reflection
// 2.Baking Environment Specular reflection

#define BAKECUBESIZE 1024

class IBL
{
public:
	IBL();
	~IBL();
	IBL(const IBL& rhs) = delete;
	IBL& operator=(const IBL& rhs) = delete;
public:
	void PreBakeGIData(RenderObject* Object);
	void Initialize();
private:
	void BakingEnvironmentDiffuse();
	void BakingEnvironmentSpecular();
private:
	unique_ptr<CubeRenderTarget> mIndirectDiffuseCube;
	unique_ptr<CubeRenderTarget> mIndirectSpecularCube;

	RenderObject* m_pSky = nullptr;
};

