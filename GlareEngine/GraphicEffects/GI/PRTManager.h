#pragma once
#include "SHProbe.h"
#include "InstanceModel/InstanceModel.h"

class PRTManager
	:public RenderObject
{
public:
	PRTManager();
	~PRTManager();

	void Initialize(AxisAlignedBox ProbeAABB, uint16_t ProbeCellSize, float ProbeDensityScale = 1.0f);

	void GenerateSHProbes(Scene& scene);
	//void SetImportantAreas();

	void CreateDebugModel(float ModelScale=0.2f);

	void DebugVisual(GraphicsContext& context);

	virtual void InitMaterial();

private:
	AxisAlignedBox mProbeAABB;
	CubeRenderTarget mCubeTarget;
	float mProbeCellSize;
	float mProbeDensityScale;
	vector<SHProbe> mSHProbes;
	ModelRenderData* mSphereMesh = nullptr;
	unique_ptr<InstanceModel> mDebugModel = nullptr;
	vector<InstanceRenderConstants> mIRC;
	//Material
	RenderMaterial* mDebugMaterial = nullptr;
};

