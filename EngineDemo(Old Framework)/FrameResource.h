#pragma once

#include "EngineUtility.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "Vertex.h"
#define InstanceCounts 25
#define MAXSubMesh 10

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT     MaterialIndex;
	UINT     ObjPad0;
	UINT     ObjPad1;
	UINT     ObjPad2;
};


struct SkinnedConstants
{
	DirectX::XMFLOAT4X4 BoneTransforms[96];
};

struct InstanceConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	UINT     MaterialIndex;
	UINT     ObjPad0;
	UINT     ObjPad1;
	UINT     ObjPad2;
};

struct MaterialData
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	UINT RoughnessSrvHeapIndex = 0;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT NormalSrvHeapIndex = 0;
	UINT MetallicSrvHeapIndex = 0;
	UINT AOSrvHeapIndex = 0;
	UINT HeightSrvHeapIndex = 0;
	float     height_scale = 0.0f;
	UINT     MaterialPad1;
	UINT     MaterialPad2;
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];

	//ALL SRV Index
	UINT     ShadowMapIndex = 0;
	UINT     WaterReflectionMapIndex = 0;
	UINT     WaterRefractionMapIndex = 0;
	UINT     WaterDumpWaveIndex = 0;


	DirectX::XMFLOAT4 FogColor;
	float  FogStart;
	float  FogRange;
	int   FogEnabled;
	int   ShadowEnabled;
};


struct TerrainConstants
{
	// When distance is minimum, the tessellation is maximum.
	// When distance is maximum, the tessellation is minimum.
	float gMinDist;
	float gMaxDist;

	// Exponents for power of 2 tessellation.  The tessellation
	// range is [2^(gMinTess), 2^(gMaxTess)].  Since the maximum
	// tessellation is 64, this means gMaxTess can be at most 6
	// since 2^6 = 64.
	float gMinTess;
	float gMaxTess;

	float gTexelCellSpaceU;
	float gTexelCellSpaceV;
	float gWorldCellSpace;
	int isReflection;

	DirectX::XMFLOAT4 gWorldFrustumPlanes[6];

	int gHeightMapIndex = 0;
	int gBlendMapIndex = 0;
	int gRGBNoiseMapIndex=0;

	float gMinWind=0.0f;
	float gMaxWind=0.0f;

	XMFLOAT3 gGrassColor;

	float gPerGrassHeight = 0.0f;
	float gPerGrassWidth = 0.0f;
	int isGrassRandom = false;
	float gWaterTransparent = 0.0f;
};



//�洢CPUΪ��ܹ��������б��������Դ��
struct FrameResource
{
public:
	
	FrameResource(ID3D12Device* device, UINT passCount,UINT InstanceModelSubMeshNum, UINT  skinnedObjectCount, UINT objectCount, UINT materialCount, UINT waveVertCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	//��GPU��ɴ�������֮ǰ�������޷����÷������� 
	//��ˣ�ÿ��֡����Ҫ���Լ��ķ�������
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	//��GPU��ɴ�������֮ǰ�������޷����÷������� 
	//��ˣ�ÿ��֡����Ҫ���Լ��ķ�������
	//std::unique_ptr<UploadBuffer<FrameConstants>> FrameCB = nullptr;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> SimpleObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<SkinnedConstants>> SkinnedCB = nullptr;
	std::unique_ptr<UploadBuffer<TerrainConstants>> TerrainCB = nullptr;
	
	std::vector<std::unique_ptr<UploadBuffer<InstanceConstants>>> InstanceSimpleObjectCB ;
	std::vector<std::unique_ptr<UploadBuffer<InstanceConstants>>> ReflectionInstanceSimpleObjectCB;
	
	// ��GPU������������������֮ǰ�������޷����¶�̬���㻺������ 
	//��ˣ�ÿ����ܶ���Ҫ�Լ��Ŀ�ܡ�
	std::unique_ptr<UploadBuffer<Vertices::PosNormalTex>> WavesVB = nullptr;

	//Χ��ֵ���Խ������ǵ���Χ���㡣 
	//��ʹ���ǿ��Լ��GPU�Ƿ�����ʹ����Щ֡��Դ�� 
	UINT64 Fence = 0;
};