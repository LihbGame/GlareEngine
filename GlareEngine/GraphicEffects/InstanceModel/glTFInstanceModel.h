#pragma once
#include "Misc/RenderObject.h"
#include "Model/Model.h"

class glTFInstanceModel :
    public RenderObject
{
public:
	glTFInstanceModel(unique_ptr<ModelInstance> model) :mModel(std::move(model)) { mObjectType = ObjectType::Model; };
	~glTFInstanceModel() {};

	virtual void Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO = nullptr) {};

	virtual void DrawShadow(GraphicsContext& Context, GraphicsPSO* SpecificShadowPSO = nullptr) {};

	virtual void DrawUI() {}

	virtual void InitMaterial();

	static void BuildPSO(const PSOCommonProperty CommonProperty);

	static uint8_t GetPSO(uint16_t psoFlags);

	void Update(float dt, GraphicsContext* Context = nullptr) { mModel->Update(*Context, dt); }

	ModelInstance* GetModel() { return mModel.get(); }

#if USE_RUNTIME_PSO

	struct ShaderNames
	{
		string PSName;
		string VSName;
	};

	static vector<ShaderNames> mShaderNames;
#endif
public:
	unique_ptr<ModelInstance> mModel;
	static 	vector<GraphicsPSO> gModelPSOs;
	static GraphicsPSO sm_PBRglTFPSO;
	static PSOCommonProperty sm_PSOCommonProperty;
	static std::mutex m_PSOMutex;

private:
	static vector<uint16_t> mPSOFlags;
	static D3D12_RASTERIZER_DESC  mMSAARasterizer;
	static D3D12_RASTERIZER_DESC  mRasterizer;
	static D3D12_BLEND_DESC  mCoverageBlend;
	static D3D12_BLEND_DESC  mBlend;
};

