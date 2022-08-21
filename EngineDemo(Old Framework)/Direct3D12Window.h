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

// ���ͽṹ�洢�����Ի�����״,�⽫��Ӧ�ó�����졣
struct RenderItem
{
	RenderItem() = default;
	//Render Item Type
	RenderItemType ItemType = RenderItemType::Normal;

	// �����������������ռ�ľֲ��ռ����״��������󣬸�����������˶����������е�λ�ã�����ͱ�����
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	//����ת������
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// ָʾ���������Ѹ��ĵ����־��������Ҫ���³����������� 
	//��Ϊ����Ϊÿ��FrameResource�ṩ��һ�����󻺳������������Ǳ��뽫����Ӧ����ÿ��FrameResource�� 
	//��ˣ��������޸Ķ�������ʱ������Ӧ������NumFramesDirty = gNumFrameResources���Ա�ÿ��֡��Դ���õ����¡�
	int NumFramesDirty = 0;

	//����������Ⱦ��Ŀ��Ӧ��ObjectCB��GPU������������
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

	// ������Ҫ�趨
	void SethWnd(HWND hWnd) { i_hWnd = hWnd; }
	void SetWinSize(int width, int height) { i_Width = width, i_Height = height; }

	virtual bool Initialize();
	virtual bool InitDirect3D();			//��ʼ��Direct3D

	virtual bool IsCreate();
	virtual void OnResize();

	virtual void Update();
	virtual void Render();

	// ��Ӧ���벿�� 
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
	// ��־����
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	void FlushCommandQueue();		//ˢ���������

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

	//������ǩ��
	void BuildRootSignature();
	//����Shader�����벼��
	void BuildShadersAndInputLayout();
	//�����򵥼�����
	void BuildSimpleGeometry();
	//�������漸���壨���Ľ���
	void BuildLandGeometry();
	//�������˼������壨���Ľ���
	void BuildWavesGeometryBuffers();
	//����PSO
	void BuildPSOs();
	//����֡��Դ
	void BuildFrameResources();
	//����������Ϣ
	void BuildAllMaterials();
	//������Ⱦ��
	void BuildRenderItems();
	void BuildModelGeoInstanceItems();

	//������Դ������
	void CreateDescriptorHeaps();

	//sampler
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	//Load model
	void LoadModel();

	//waves
	float GetHillsHeight(float x, float z)const;


	//������Ⱦ��
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
	//��Ϸʱ�����
	GameTimer mTimer;
	
	// ��߱�
	float     AspectRatio()const;

private:

	HWND i_hWnd = nullptr;
	int i_Width = 0;
	int i_Height = 0;

	// Set true to use 4X MSAA (?.1.8).  The default is false.
	bool      m4xMsaaState = false;    // 4X MSAA enabled
	UINT      m4xMsaaQuality = 1;      // 4X MSAA������ˮƽ

	// ֡��Դ
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

	// ͬ������
	ComPtr<ID3D12Fence> Fence;
	UINT64 mCurrentFence = 0;

	UINT RtvDescriptorSize = 0;
	UINT DsvDescriptorSize = 0;
	UINT CbvSrvUavDescriptorSize = 0;

	// ������Ӧ���������캯���������������Զ�����ʼֵ��
	DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;//���ֵ����

	// ���ڴ�С�Ͳ��д�С
	CD3DX12_VIEWPORT ScreenViewport;
	CD3DX12_RECT ScissorRect;

	// ---------------------------------------------------
	static const int SwapChainBufferCount = 3; // ������
	int mCurrBackBuffer = 0; // �󻺳�
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

	//���
	Camera Camera;

	//TEXTURE MANAGER
	std::unique_ptr<TextureManage> textureManage;
	int mSRVSize = 0;
	std::vector<std::wstring> mPBRTextureName;

	// �����
	std::unique_ptr<Sky> sky;
	int mSkyMapIndex = 0;

	// ʵ���޳�
	bool mFrustumCullingEnabled = true;
	BoundingFrustum mCamFrustum;
	std::unique_ptr<SimpleGeoInstance> simpleGeoInstance;

	// ��Ӱ��ͼ
	std::unique_ptr<ShadowMap> shadowMap;
	int mShadowMapIndex = 0;
	bool RedrawShadowMap = true;
	int ShadowMapSize = 4096;

	// ģ�ͼ�����
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