#pragma once
#include "Misc/CubeRenderTarget.h"
#include "Misc/RenderObject.h"
#include "Engine/RenderMaterial.h"

// Preview:
// 1.Baking Environment Diffuse reflection
// 2.Baking Environment Specular reflection

#define BAKECUBESIZE 1024

class IBL:
	public RenderObject
{
public:
	IBL();
	~IBL();
	IBL(const IBL& rhs) = delete;
	IBL& operator=(const IBL& rhs) = delete;
public:
	void PreBakeGIData(GraphicsContext& Context, RenderObject* Object);
	void Initialize();
	virtual void InitMaterial();
private:
	void BakingEnvironmentDiffuse(GraphicsContext& Context);
	void BakingPreFilteredEnvironment(GraphicsContext& Context);
	void BakingBRDF(GraphicsContext& Context);
	void SaveBakingDataToFiles(GraphicsContext& Context);
private:
	unique_ptr<CubeRenderTarget> mIndirectDiffuseCube;
	unique_ptr<CubeRenderTarget> mPreFilteredEnvCube;
	ColorBuffer mBRDFLUT;

	RenderObject* m_pSky = nullptr;

	static RootSignature* m_pRootSignature;
	RenderMaterial* mIndirectDiffuseMaterial = nullptr;
	RenderMaterial* mPreFilteredEnvMapMaterial = nullptr;
	RenderMaterial* mBRDFMaterial = nullptr;
};

