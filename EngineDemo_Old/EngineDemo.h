#pragma once

#include "resource.h"
#include "BaseApp.h"
#include "Math/MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Water/Waves.h"
#include "FrameResource.h"
#include "Material.h"

//shader head files
#include "BaseShader.h"
#include "GerstnerWaveShader.h"
#include "SkyShader.h"
#include "SimpleGeometryInstanceShader.h"
#include "SimpleGeometryShadowMapShader.h"
#include "ComplexStaticModelInstanceShader.h"
#include "SkinAnimeShader.h"
#include "WaterRefractionMaskShader.h"
#include "ShockWaveWaterShader.h"
#include "HeightMapTerrainShader.h"
#include "GrassShader.h"

#include "PSOManager.h"
#include "Engine/Camera.h"
#include "TextureManage.H"
#include "HDRSky/Sky.h"
#include "EngineGUI.h"
#include "Shadow/ShadowMap.h"
#include "Animation/ModelLoader.h"
#include "SimpleGeoInstance/SimpleGeoInstance.h"
#include "ShockWaveWater/ShockWaveWater.h"
#include "Terrain/HeightmapTerrain.h"
//#include "OzzAnimePlayBack.h"
#include "Engine/EngineLog.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

enum RenderItemType :int
{
	Normal = 0,
	Reflection
};

// 轻型结构存储参数以绘制形状,这将因应用程序而异。
struct RenderItem
{
	RenderItem() = default;
	//Render Item Type
	RenderItemType ItemType = RenderItemType::Normal;

	// 描述对象相对于世界空间的局部空间的形状的世界矩阵，该世界矩阵定义了对象在世界中的位置，方向和比例。
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	//纹理转换矩阵。
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// 指示对象数据已更改的脏标志，我们需要更新常量缓冲区。 
	//因为我们为每个FrameResource提供了一个对象缓冲区，所以我们必须将更新应用于每个FrameResource。 
	//因此，当我们修改对象数据时，我们应该设置NumFramesDirty = gNumFrameResources，以便每个帧资源都得到更新。
	int NumFramesDirty = gNumFrame;

	//索引到此渲染项目对应于ObjectCB的GPU常量缓冲区。
	int ObjCBIndex = -1;

	::Material* Mat = nullptr;
	std::vector<::MeshGeometry*> Geo;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//Instance Data
	BoundingBox Bounds;
	std::vector<InstanceConstants> Instances;


	// DrawIndexedInstanced parameters.
	UINT InstanceCount;
	std::vector<UINT> IndexCount;
	std::vector<UINT> VertexCount;
	std::vector<UINT> StartIndexLocation;
	std::vector<int> BaseVertexLocation;
};

enum class RenderLayer : int
{
	Opaque = 0,
	InstanceSimpleItems,
	Sky,
	ShockWaveWater,
	HeightMapTerrain,
	Grass,
	Count
};



//APPLICATION
class GameApp : public D3DApp
{
public:
	GameApp(HINSTANCE hInstance);
	GameApp(const GameApp& rhs) = delete;
	GameApp& operator=(const GameApp& rhs) = delete;
	~GameApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void CreateRtvAndDsvDescriptorHeaps()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	//Draw all things
	virtual void Draw(const GameTimer& gt)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdateCamera(const GameTimer& gt);

	void UpdateObjectCBs(const GameTimer& gt);

	void UpdateInstanceCBs(const GameTimer& gt);

	void UpdateMaterialCBs(const GameTimer& gt);

	void UpdateMainPassCB(const GameTimer& gt);

	void UpdateWaves(const GameTimer& gt);

	void UpdateShadowPassCB(const GameTimer& gt);

	void UpdateTerrainPassCB(const GameTimer& gt);

	void UpdateAnimation(const GameTimer& gt);



	//创建根签名
	void BuildRootSignature();
	//创建Shader和输入布局
	void BuildShadersAndInputLayout();
	//创建简单几何体
	void BuildSimpleGeometry();
	//创建地面几何体（待改进）
	void BuildLandGeometry();
	//创建波浪集几何体（待改进）
	void BuildWavesGeometryBuffers();
	//创建PSO
	void BuildPSOs();
	//创建帧资源
	void BuildFrameResources();
	//创建材质信息
	void BuildAllMaterials();
	//创建渲染项
	void BuildRenderItems();
	void BuildModelGeoInstanceItems();

	//创建资源描述堆
	void CreateDescriptorHeaps();
	void CreatePBRSRVinDescriptorHeap(vector<ID3D12Resource*> TexResource, int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor, wstring MaterialName);

	//sampler
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	//Load model
	void LoadModel();

	//waves
	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;


	//绘制渲染项
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems, bool IsIndexInstanceDraw = true);
	//Draw Scene To ShadowMap
	void DrawSceneToShadowMap(const GameTimer& gr);
	//Draw Shock Wave Water
	void DrawShockWaveWater(const GameTimer& gt);
	void DrawWaterReflectionMap(const GameTimer& gt);
	void DrawWaterRefractionMap(const GameTimer& gt);
	//Draw Height map terrain
	void DrawHeightMapTerrain(const GameTimer& gr, bool IsReflection = false);
	//Draw Grass
	void DrawGrass(const GameTimer& gr, bool IsReflection = false);

	//Height map terrain
	HeightmapTerrain::InitInfo HeightmapTerrainInit();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mGUISrvDescriptorHeap = nullptr;


	std::unordered_map<std::string, std::unique_ptr<::MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;


	std::unique_ptr<PSO> mPSOs;
	//Shaders
	std::unordered_map<std::string, BaseShader*> mShaders;



	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	RenderItem* mSphereRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<RenderItem*> mReflectionWaterLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

	PassConstants mMainPassCB;  // index 0 of pass cbuffer.
	PassConstants mShadowPassCB;// index 1 of pass cbuffer.
	TerrainConstants mTerrainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV2 - 0.1f;
	float mRadius = 500.0f;

	float mSunTheta = 1.25f * XM_PI;
	float mSunPhi = XM_PIDIV4;

	POINT mLastMousePos;

	//相机
	Camera mCamera;
	//TEXTURE MANAGER
	std::unique_ptr<TextureManage> mTextureManage;
	int mSRVSize = 0;
	std::vector<wstring> mPBRTextureName;
	//sky
	std::unique_ptr<Sky> mSky;

	int mSkyMapIndex = 0;
	//UI
	std::unique_ptr<EngineGUI> mEngineUI;

	//Instance Culling
	bool mFrustumCullingEnabled = true;
	BoundingFrustum mCamFrustum;
	std::unique_ptr<SimpleGeoInstance> mSimpleGeoInstance;

	//shadow map 
	std::unique_ptr<ShadowMap> mShadowMap;
	int mShadowMapIndex = 0;
	static bool RedrawShadowMap;

	//Model Loader
	std::unique_ptr<ModelLoader> mModelLoder;

	//animation transform
	std::vector<XMFLOAT4X4> transforms;

	//ShockWaveWater
	std::unique_ptr<ShockWaveWater> mShockWaveWater;
	int mWaterReflectionMapIndex = 0;
	int mWaterRefractionMapIndex = 0;
	int mWaterDumpWaveIndex = 0;

	//Terrain
	std::unique_ptr<HeightmapTerrain> mHeightMapTerrain;
	TerrainConstants  mTerrainConstant;
	int mBlendMapIndex = 0;
	int mHeightMapIndex = 0;
	int mRGBNoiseMapIndex = 0;
	//play back anime 
	//std::unordered_map<std::string, AnimePlayback> AnimationPlayback;

	//sizeof ObjectConstants 
	UINT mObjCBByteSize;
	UINT mSkinnedCBByteSize;
	UINT mTerrainCBByteSize;

	bool mOldWireFrameState = false;
	bool mNewWireFrameState = false;
};