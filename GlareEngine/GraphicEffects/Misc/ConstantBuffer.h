#pragma once
#include "Engine/EngineUtility.h"
#include "Math/MathHelper.h"
#include "Engine/Vertex.h"
#include "Math/Matrix3.h"
#include "Math/Matrix4.h"

// The order of textures for PBR materials
enum 
{ 
	eBaseColor, 
	eMetallicRoughness, 
	eOcclusion, 
	eEmissive, 
	eNormal, 
	eClearCoat,
	eNumTextures 
};


__declspec(align(256)) struct MeshConstants
{
	Matrix4 World;         // Object to world
	Matrix3 WorldIT;       // Object normal to world normal
};

__declspec(align(256)) struct MaterialConstants
{
	float baseColorFactor[4];			// default=[1,1,1,1]
	float emissiveFactor[3];			// default=[0,0,0]
	float normalTextureScale;			// default=1
	float metallicFactor;				// default=1
	float roughnessFactor;				// default=1
	float clearCoatFactor;				// default=1
	union
	{
		uint32_t flags;
		struct
		{
			// UV0 or UV1 for each texture
			uint32_t baseColorUV : 1;
			uint32_t metallicRoughnessUV : 1;
			uint32_t occlusionUV : 1;
			uint32_t emissiveUV : 1;
			uint32_t normalUV : 1;

			//Special modes
			uint32_t twoSided : 1;
			uint32_t alphaTest : 1;
			uint32_t alphaBlend : 1;

			uint32_t _pad : 8; // unused

			uint32_t alphaRef : 16; // half float
		};
	};

	union
	{
		uint32_t Specialflags;
		struct
		{
			// UV0 or UV1 for each texture
			uint32_t clearCoatUV : 1;

			//Special modes
			uint32_t clearCoat : 1;

			uint32_t _pad : 30; // unused
		};
	};
};

struct InstanceConstants
{
	void* mDataPtr;
	uint32_t mDataSize; //Size if Instance Render Constants
	uint32_t mDataNum;  //Number if Instance Render Constants
};

struct InstanceRenderConstants
{
	XMFLOAT4X4		mWorldTransform		= MathHelper::Identity4x4();
	XMFLOAT4X4		mTexTransform		= MathHelper::Identity4x4();
	int				mMaterialIndex;
	int				mPAD1;
	int				mPAD2;
	int				mPAD3;
};


__declspec(align(16))struct MainConstants
{
	XMFLOAT4X4	View					= MathHelper::Identity4x4();
	XMFLOAT4X4	InvView					= MathHelper::Identity4x4();
	XMFLOAT4X4	Proj					= MathHelper::Identity4x4();
	XMFLOAT4X4	InvProj					= MathHelper::Identity4x4();
	XMFLOAT4X4	ViewProj				= MathHelper::Identity4x4();
	XMFLOAT4X4	InvViewProj				= MathHelper::Identity4x4();
	XMFLOAT4X4	ShadowTransform			= MathHelper::Identity4x4();
	XMFLOAT4X4	PreViewProjMatrix		= MathHelper::Identity4x4();

	XMFLOAT3	EyePosW					= { 0.0f, 0.0f, 0.0f };
	float		ZMagic					= 0.0f;     // (zFar - zNear) / zNear

	XMFLOAT2	RenderTargetSize		= { 0.0f, 0.0f };
	XMFLOAT2	InvRenderTargetSize		= { 0.0f, 0.0f };

	float		NearZ					= 0.0f;
	float		FarZ					= 0.0f;
	float		TotalTime				= 0.0f;
	float		DeltaTime				= 0.0f;

	XMFLOAT4	gAmbientLight			= { 0.0f, 0.0f, 0.0f ,0.0f };

	DirectionalLight Lights[MAX_DIRECTIONAL_LIGHTS];

	float		InvTileDim[4];
	int			TileCount[4];

	//ShadowMap Index
	int			mShadowMapIndex					= 0;
	int			mSkyCubeIndex					= 0;
	int			mBakingDiffuseCubeIndex			= 0;
	int			gBakingPreFilteredEnvIndex		= 0;

	int			gBakingIntegrationBRDFIndex		= 0;
	float		IBLRange						= 10.0f;
	float		IBLBias							= 0.0f;
	int			gIsClusterBaseLighting			= 0;

	XMFLOAT2	gTemporalJitter					= { 0.0f, 0.0f };
	int			gDirectionalLightsCount			= 0;
	int			gIsIndoorScene					= 0;

	XMFLOAT2    gCluserFactor					= { 0.0f, 0.0f };
	XMFLOAT2    gPerTileSize					= { 0.0f, 0.0f };

	XMFLOAT3    gTileSizes						= { 0.0f, 0.0f, 0.0f };
	int			IsRenderTiledBaseLighting		= 1;

	int			bEnablePointLight;
	int 		bEnableAreaLight;
	int 		bEnableConeLight;
	int 		bEnableDirectionalLight;

	int			gAreaLightLTC1SRVIndex;
	int			gAreaLightLTC2SRVIndex;
	int         gAreaLightTwoSide;
	float       gMipLODBias;

	XMFLOAT2    gCurJitterOffset;
	XMFLOAT2    gPreJitterOffset;
};



struct TerrainConstants
{
	XMFLOAT4	gWorldFrustumPlanes[6];

	float		gTessellationScale;
	float		gTexelCellSpaceU;
	float		gTexelCellSpaceV;
	float		gWorldCellSpace;

	int			gHeightMapIndex		= 0;
	int			gBlendMapIndex		= 0;
	int			gRGBNoiseMapIndex	= 0;
};



// Procedural terrain constants for GPU generation
__declspec(align(256)) struct ProceduralTerrainNoiseCB
{
    XMFLOAT3    CameraPosition      = { 0,0,0 };
    float       CellSize            = 1.0f;
    XMINT2      TileOffset          = { 0, 0 };
    int         TileSize            = 64;
    int         HeightmapSize       = 128;
    float       HeightScale         = 200.0f;
    float       NoiseScale          = 0.05f;
    UINT        Seed                = 42;
    UINT        Octaves             = 7;
    float       Lacunarity          = 2.0f;
    float       Persistence         = 0.5f;
    float       WarpStrength        = 30.0f;
    float       WarpScale           = 0.02f;
    float       SnowHeight          = 150.0f;
    float       SnowTransition      = 20.0f;
    float       StoneSlope          = 0.6f;
    float       StoneTransition     = 0.15f;
    int         LODLevel            = 0;
    float       Pad0                = 0;
    float       Pad1                = 0;
    float       Pad2                = 0;
};

// Procedural terrain render constants
__declspec(align(256)) struct ProceduralTerrainConstants
{
    XMFLOAT4X4  ViewProj            = MathHelper::Identity4x4();
    XMFLOAT3    EyePosW             = { 0,0,0 };
    float       MinTessDistance     = 10.0f;
    float       MaxTessDistance     = 500.0f;
    float       MinTessFactor       = 2.0f;
    float       MaxTessFactor       = 6.0f;
    int         ClipmapLevel        = 0;
    float       CellSize            = 1.0f;
    float       HeightScale         = 200.0f;
    float       TexScale            = 50.0f;
    float       StochasticSharpness = 0.95f;
    XMFLOAT4    gWorldFrustumPlanes[6];
    // Per-tile SRV indices set dynamically
    int         HeightMapIndex      = 0;
    int         NormalMapIndex      = 0;
    int         MaterialWeightMapIndex = 0;
    // Per-tile positioning data (separate ints for HLSL packing alignment)
    int         TileGridOffsetX    = 0;
    int         TileGridOffsetY    = 0;
    int         _PadTileOffset[3]  = {}; // Padding for int4 16-byte alignment
    // Material layer SRV indices (padded to match HLSL cbuffer array packing:
    // HLSL pads each int array element to 16 bytes, so int[5] = 80 bytes)
    struct { int Index; int _Pad[3]; } LayerAlbedoIndices[5]    = {};
    struct { int Index; int _Pad[3]; } LayerNormalIndices[5]    = {};
    struct { int Index; int _Pad[3]; } LayerRoughnessIndices[5] = {};
    struct { int Index; int _Pad[3]; } LayerMetallicIndices[5]  = {};
    struct { int Index; int _Pad[3]; } LayerAOIndices[5]        = {};
    // Finer LOD level coverage bounds for patch-level clipping in HS
    int         HasFinerLevel            = 0;
    float       FinerLevelMinX           = 0.0f;
    float       FinerLevelMaxX           = 0.0f;
    float       FinerLevelMinZ           = 0.0f;
    float       FinerLevelMaxZ           = 0.0f;
    float       RoughnessScale           = 3.0f;
    float       MetallicScale            = 0.3f;
    // HLSL packs float2 _PadMV to next 16-byte row (offset 672),
    // and float4x4 gTerrainPreViewProj needs 16-byte alignment (offset 688).
    // Explicit padding matches HLSL cbuffer layout.
    float       _PadRow41               = 0; // pad current 16-byte row
    float       _PadMV[2]                = {};
    float       _PadBeforePreVP[2]       = {}; // pad to 16-byte align PreViewProj
    // Previous frame data for motion vector computation
    XMFLOAT4X4  PreViewProj              = MathHelper::Identity4x4();
    XMFLOAT2    PreJitterOffset          = { 0.0f, 0.0f };
    XMFLOAT2    CurJitterOffset          = { 0.0f, 0.0f };
};

struct ProceduralTerrainInitInfo
{
    UINT        ClipmapLevels       = 10;
    UINT        TileSize            = 64;
    UINT        HeightmapSize       = 128;
    float       CellSizeBase        = 1.0f;
    float       HeightScale         = 2000.0f;
    float       NoiseScale          = 0.001f;
    UINT        Seed                = 42;
    string      LayerMapNames[5]    = { "grass","lightdirt","darkdirt","stone","snow" };
    string      LayerAssetPath;
};


struct CubeMapConstants
{
	XMFLOAT4X4	View				= MathHelper::Identity4x4();
	XMFLOAT4X4	Proj				= MathHelper::Identity4x4();
	XMFLOAT4X4	ViewProj			= MathHelper::Identity4x4();
	XMFLOAT3	EyePosW				= { 0.0f, 0.0f, 0.0f };
	float		cbPerObjectPad1		= 0.0f;
	XMFLOAT2	RenderTargetSize	= { 0.0f, 0.0f };
	XMFLOAT2	InvRenderTargetSize = { 0.0f, 0.0f };
	int			mSkyCubeIndex		= 0;
	float		mRoughness			= 0;
	int			mPad02				= 0;
	int			mPad03				= 0;
};


struct GlobleSRVIndex
{
	static int gSkyCubeSRVIndex;
	static int gBakingDiffuseCubeIndex;
	static int gBakingPreFilteredEnvIndex;
	static int gBakingIntegrationBRDFIndex;
};
