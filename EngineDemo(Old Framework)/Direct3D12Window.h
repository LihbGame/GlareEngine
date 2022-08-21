#pragma once

#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Waves.h"
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
#include "Camera.h"
#include "TextureManage.H"
#include "Sky.h"
#include "EngineGUI.h"
#include "ShadowMap.h"
#include "ModelLoader.h"
#include "SimpleGeoInstance.h"
#include "ShockWaveWater.h"
#include "HeightmapTerrain.h"
//#include "OzzAnimePlayBack.h"
#include "EngineLog.h"

using namespace Microsoft::WRL;
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
	int NumFramesDirty = 0;

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
class Direct3D12Window
{
public:
	Direct3D12Window();
	~Direct3D12Window();

	// 两大主要设定
	void SethWnd(HWND hWnd) { i_hWnd = hWnd; }
	void SetWinSize(int width, int height) { i_Width = width, i_Height = height; }

	virtual bool Initialize();
	virtual bool InitDirect3D();			//初始化Direct3D

	virtual bool IsCreate();
	virtual void OnResize();

	virtual void Update();
	virtual void Render();

	// 响应输入部分 
	virtual void OnLButtonDown(WPARAM btnState, int x, int y);
	virtual void OnLButtonUp(WPARAM btnState, int x, int y);
	virtual void OnRButtonDown(WPARAM btnState, int x, int y);
	virtual void OnRButtonUp(WPARAM btnState, int x, int y);
	virtual void OnMButtonDown(WPARAM btnState, int x, int y);
	virtual void OnMButtonUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	virtual void OnMouseWheel(WPARAM btnState, int x, int y);

	virtual void ActiveSystemDesktopSwap();
	virtual void CalculateFrameStats();

	virtual void OnKeyDown(UINT8 key);
	virtual void OnKeyUp(UINT8 key);

private:
	// 日志部分
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	void FlushCommandQueue();		//刷新命令队列

	void UpdateCamera();

	void UpdateObjectCBs();

	void UpdateInstanceCBs();

	void UpdateMaterialCBs();

	void UpdateMainPassCB();

	void UpdateWaves();

	void UpdateShadowPassCB();

	void UpdateTerrainPassCB();

	void UpdateAnimation();

	void CreateDirectDevice();
	void CreateCommandQueueAndSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();

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

	//sampler
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	//Load model
	void LoadModel();

	//waves
	float GetHillsHeight(float x, float z)const;


	//绘制渲染项
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems, bool IsIndexInstanceDraw = true);
	//Draw Scene To ShadowMap
	void DrawSceneToShadowMap();
	//Draw Shock Wave Water
	void DrawShockWaveWater();
	void DrawWaterReflectionMap();
	void DrawWaterRefractionMap();
	//Draw Height map terrain
	void DrawHeightMapTerrain(bool IsReflection = false);
	//Draw Grass
	void DrawGrass(bool IsReflection = false);

	//Height map terrain
	HeightmapTerrain::InitInfo HeightmapTerrainInit();

public:
	//游戏时间管理
	GameTimer mTimer;
	
	// 宽高比
	float     AspectRatio()const;

private:

	HWND i_hWnd = nullptr;
	int i_Width = 0;
	int i_Height = 0;

	// Set true to use 4X MSAA (?.1.8).  The default is false.
	bool      m4xMsaaState = false;    // 4X MSAA enabled
	UINT      m4xMsaaQuality = 1;      // 4X MSAA的质量水平

	// 帧资源
	const int gNumFrameResources = 3;
	std::vector<std::unique_ptr<FrameResource>> FrameResources;
	FrameResource* CurrFrameResource = nullptr;
	int CurrFrameResourceIndex = 0;

	UINT CbvSrvDescriptorSize = 0;

	// ---------------------------------------------------
	ComPtr<IDXGIFactory4> DXGIFactory = nullptr;
	ComPtr<IDXGIAdapter1> DeviceAdapter = nullptr;
	ComPtr<IDXGISwapChain> SwapChain = nullptr;
	ComPtr<ID3D12Device> d3dDevice = nullptr;
	ComPtr<ID3D12RootSignature> RootSignature = nullptr;
	ComPtr<ID3D12CommandQueue> CommandQueue = nullptr;
	ComPtr<ID3D12CommandAllocator> DirectCmdListAlloc = nullptr;
	ComPtr<ID3D12GraphicsCommandList5> CommandList = nullptr;

	// 同步对象。
	ComPtr<ID3D12Fence> Fence;
	UINT64 mCurrentFence = 0;

	UINT RtvDescriptorSize = 0;
	UINT DsvDescriptorSize = 0;
	UINT CbvSrvUavDescriptorSize = 0;

	// 派生类应在派生构造函数中设置它们以自定义起始值。
	DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;//深度值类型

	// 窗口大小和裁切大小
	CD3DX12_VIEWPORT ScreenViewport;
	CD3DX12_RECT ScissorRect;

	// ---------------------------------------------------
	static const int SwapChainBufferCount = 3; // 三缓冲
	int mCurrBackBuffer = 0; // 后缓冲
	ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount] = { nullptr };
	ComPtr<ID3D12Resource> DepthStencilBuffer = nullptr;
	ComPtr<ID3D12Resource> MSAARenderTargetBuffer = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mGUISrvDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> RTVDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> MSAARTVDescriptorHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> DSVDescriptorHeap = nullptr;
	// ---------------------------------------------------

	std::unordered_map<std::wstring, std::unique_ptr<::MeshGeometry>> mGeometries;
	std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;


	std::unique_ptr<PSO> PSOs;

	//Shaders
	std::unordered_map<std::wstring, BaseShader*> Shaders;


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

	POINT mLastMousePos = { 0,0 };

	//UI
	EngineGUI EngineUI;

	//相机
	Camera Camera;

	//TEXTURE MANAGER
	std::unique_ptr<TextureManage> textureManage;
	int mSRVSize = 0;
	std::vector<std::wstring> mPBRTextureName;

	// 天空球
	std::unique_ptr<Sky> sky;
	int mSkyMapIndex = 0;

	// 实例剔除
	bool mFrustumCullingEnabled = true;
	BoundingFrustum mCamFrustum;
	std::unique_ptr<SimpleGeoInstance> simpleGeoInstance;

	// 阴影贴图
	std::unique_ptr<ShadowMap> shadowMap;
	int mShadowMapIndex = 0;
	bool RedrawShadowMap = true;
	int ShadowMapSize = 4096;

	// 模型加载器
	std::unique_ptr<ModelLoader> modelLoder;

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

	//sizeof ObjectConstants 
	UINT mObjCBByteSize;
	UINT mSkinnedCBByteSize;
	UINT mTerrainCBByteSize;

	bool mOldWireFrameState = false;
	bool mNewWireFrameState = false;
};