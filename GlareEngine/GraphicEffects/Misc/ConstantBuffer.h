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
	XMFLOAT4X4	CSMShadowTransform[4]	= {
		MathHelper::Identity4x4(), MathHelper::Identity4x4(),
		MathHelper::Identity4x4(), MathHelper::Identity4x4()
	};
	XMFLOAT4	CSMCascadeSplits		= { 0.0f, 0.0f, 0.0f, 0.0f };
	XMINT4		CSMShadowMapIndices		= { 0, 0, 0, 0 };
	int			CSMCascadeCount			= 1;
	int			_CSMPad0				= 0;
	int			_CSMPad1				= 0;
	int			_CSMPad2				= 0;

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

	float       gShadowIntensity = 1.0f;
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



constexpr UINT kTerrainNoiseMaxBaseLayers = 4;
constexpr UINT kTerrainNoiseMaxDetailLayers = 4;
constexpr UINT kTerrainNoiseMaxLayers = kTerrainNoiseMaxBaseLayers + kTerrainNoiseMaxDetailLayers;
constexpr UINT kTerrainFilterMaxLayers = 4;

enum TerrainNoiseType : int
{
    TerrainNoise_Billow = 0,
    TerrainNoise_Gabor,
    TerrainNoise_Perlin,
    TerrainNoise_Phasor,
    TerrainNoise_Ridged,
    TerrainNoise_Simplex,
    TerrainNoise_Value,
    TerrainNoise_Voronoi,
    TerrainNoise_Wave,
    TerrainNoise_White,
    TerrainNoise_Worley,
    TerrainNoise_Count
};

enum TerrainNoiseFractalType : int
{
    TerrainFractal_Single = 0,
    TerrainFractal_FBM,
    TerrainFractal_Ridged,
    TerrainFractal_Count
};

enum TerrainNoiseWarpMode : int
{
    TerrainWarp_None = 0,
    TerrainWarp_Fixed,
    TerrainWarp_Recursive,
    TerrainWarp_Count
};

enum TerrainNoiseCombineOp : int
{
    TerrainCombine_Add = 0,
    TerrainCombine_Multiply,
    TerrainCombine_Subtract,
    TerrainCombine_Min,
    TerrainCombine_Max,
    TerrainCombine_Blend,
    TerrainCombine_Count
};

enum TerrainNoisePlacementMode : int
{
    TerrainPlacement_World = 0,
    TerrainPlacement_Rotated,
    TerrainPlacement_Mirrored,
    TerrainPlacement_Count
};

enum TerrainFilterType : int
{
    TerrainFilter_None = 0,
    TerrainFilter_Smooth,
    TerrainFilter_Terrace,
    TerrainFilter_Strata,
    TerrainFilter_Distortion,
    TerrainFilter_SedimentFill,
    TerrainFilter_Count
};

struct TerrainNoiseLayerSettings
{
    bool        Enabled        = false;
    int         NoiseType      = TerrainNoise_Perlin;
    int         FractalType    = TerrainFractal_FBM;
    int         WarpMode       = TerrainWarp_None;
    int         CombineOp      = TerrainCombine_Add;
    int         PlacementMode  = TerrainPlacement_World;
    int         Octaves        = 4;
    int         VoronoiMode    = 0;
    float       Opacity        = 1.0f;
    float       Amplitude      = 1.0f;
    float       Frequency      = 1.0f;
    float       Lacunarity     = 2.0f;
    float       Gain           = 0.5f;
    float       WarpAmplitude  = 0.0f;
    float       WarpFrequency  = 1.0f;
    float       Rotation       = 0.0f;
    XMFLOAT2    Offset         = { 0.0f, 0.0f };
    XMFLOAT2    Scale          = { 1.0f, 1.0f };
};

struct TerrainFilterSettings
{
    bool        Enabled        = false;
    int         FilterType     = TerrainFilter_None;
    int         CombineOp      = TerrainCombine_Blend;
    int         Iterations     = 1;
    float       Strength       = 1.0f;
    float       Radius         = 16.0f;
    float       Param0         = 0.0f;
    float       Param1         = 0.0f;
    float       Param2         = 0.0f;
    float       Param3         = 0.0f;
};

// Procedural terrain constants for GPU generation
__declspec(align(256)) struct ProceduralTerrainNoiseCB
{
    XMFLOAT3    CameraPosition      = { 0,0,0 };
    float       CellSize            = 1.0f;
    XMINT2      TileOffset          = { 0, 0 };
    int         TileSize            = 64;
    int         HeightmapSize       = 256;
    float       HeightScale         = 200.0f;
    UINT        Seed                = 42;
    float       PadNoiseHeader0     = 0.0f;
    float       PadNoiseHeader1     = 0.0f;
    float       PadNoiseHeader2     = 0.0f;
    float       PadNoiseHeader3     = 0.0f;
    float       PadNoiseHeader4     = 0.0f;
    float       PadNoiseHeader5     = 0.0f;
    XMINT4      LayerControls[kTerrainNoiseMaxLayers] = {};   // enabled, noise type, fractal type, combine op
    XMINT4      LayerOptions[kTerrainNoiseMaxLayers]  = {};   // warp mode, octaves, placement mode, voronoi mode
    XMFLOAT4    LayerShape[kTerrainNoiseMaxLayers]    = {};   // opacity, amplitude, frequency, lacunarity
    XMFLOAT4    LayerWarp[kTerrainNoiseMaxLayers]     = {};   // gain, warp amplitude, warp frequency, rotation
    XMFLOAT4    LayerPlacement[kTerrainNoiseMaxLayers] = {};  // offset xz, scale xz
    XMINT4      LayerCounts        = {
        (int)kTerrainNoiseMaxBaseLayers,
        (int)kTerrainNoiseMaxDetailLayers,
        0,
        0
    };
    XMINT4      FilterControls[kTerrainFilterMaxLayers] = {}; // enabled, type, combine op, iterations
    XMFLOAT4    FilterParams0[kTerrainFilterMaxLayers]  = {}; // strength, radius, param0, param1
    XMFLOAT4    FilterParams1[kTerrainFilterMaxLayers]  = {}; // param2, param3, reserved, reserved
    XMINT4      FilterCounts        = {
        (int)kTerrainFilterMaxLayers,
        0,
        0,
        0
    };
    float       SnowHeight          = 150.0f;
    float       SnowTransition      = 20.0f;
    float       StoneSlope          = 0.6f;
    float       StoneTransition     = 0.15f;
    int         LODLevel            = 0;
    int         PadNoiseFooter0     = 0;
    float       PadNoiseFooter1     = 0.0f;
    float       PadNoiseFooter2     = 0.0f;
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
    float       TessScale           = 1.0f;
    int         ClipmapLevel        = 0;
    float       CellSize            = 1.0f;
    float       HeightScale         = 200.0f;
    float       TexScale            = 50.0f;
    float       StochasticSharpness = 0.35f;
    float       _PadTessScale[3]    = {};
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
    float       SkirtDepth               = 50.0f;
    UINT        SkirtEdgeFlags           = 0;
    // Pad to match HLSL cbuffer: RoughnessScale + MetallicScale + SkirtDepth + SkirtEdgeFlags
    // fill one 16-byte word, then _PadMV(float2) + implicit pad fills the next.
    float       _PadMV[3]                = {};
    // Previous frame data for motion vector computation
    XMFLOAT4X4  PreViewProj              = MathHelper::Identity4x4();
    XMFLOAT2    PreJitterOffset          = { 0.0f, 0.0f };
    XMFLOAT2    CurJitterOffset          = { 0.0f, 0.0f };
    // Detail texture parameters
    float       DetailScale              = 20.0f;
    float       DetailFadeDistance       = 50.0f;
    int         UseHeightBlend           = 1;
    int         UseTriplanar             = 1;
    float       ParallaxHeightScale      = 0.012f;
    int         UseTerrainParallax       = 1;
    float       _PadParallax[2]          = {};
    struct { int Index; int _Pad[3]; } DetailAlbedoIndices[5]    = {};
    struct { int Index; int _Pad[3]; } DetailNormalIndices[5]    = {};
    struct { int Index; int _Pad[3]; } DetailRoughnessIndices[5] = {};
    struct { int Index = -1; int _Pad[3] = {}; } LayerHeightIndices[5];
};

struct ProceduralTerrainInitInfo
{
    UINT        ClipmapLevels       = 10;
    UINT        TileSize            = 64;
    UINT        HeightmapSize       = 256;
    float       CellSizeBase        = 1.0f;
    float       HeightScale         = 500.0f;
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
