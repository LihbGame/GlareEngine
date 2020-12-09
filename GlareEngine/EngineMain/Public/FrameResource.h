#pragma once

#include "L3DUtil.h"
#include "L3DMathHelper.h"
#include "L3DUploadBuffer.h"
#include "L3DVertex.h"
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
};



//存储CPU为框架构建命令列表所需的资源。
struct FrameResource
{
public:
    
    FrameResource(ID3D12Device* device, UINT passCount,UINT InstanceModelSubMeshNum, UINT  skinnedObjectCount, UINT objectCount, UINT materialCount, UINT waveVertCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    //在GPU完成处理命令之前，我们无法重置分配器。 
    //因此，每个帧都需要有自己的分配器。
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    //在GPU完成处理命令之前，我们无法重置分配器。 
    //因此，每个帧都需要有自己的分配器。
    //std::unique_ptr<UploadBuffer<FrameConstants>> FrameCB = nullptr;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> SimpleObjectCB = nullptr;
    std::unique_ptr<UploadBuffer<SkinnedConstants>> SkinnedCB = nullptr;
    std::unique_ptr<UploadBuffer<TerrainConstants>> TerrainCB = nullptr;
    
    vector<std::unique_ptr<UploadBuffer<InstanceConstants>>> InstanceSimpleObjectCB ;
    vector<std::unique_ptr<UploadBuffer<InstanceConstants>>> ReflectionInstanceSimpleObjectCB;
    
    // 在GPU处理完引用它的命令之前，我们无法更新动态顶点缓冲区。 
    //因此，每个框架都需要自己的框架。
    std::unique_ptr<UploadBuffer<L3DVertice::PosNormalTexc>> WavesVB = nullptr;

    //围栏值，以将命令标记到该围栏点。 
    //这使我们可以检查GPU是否仍在使用这些帧资源。 
    UINT64 Fence = 0;
};