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

			// Three special modes
			uint32_t twoSided : 1;
			uint32_t alphaTest : 1;
			uint32_t alphaBlend : 1;

			uint32_t _pad : 8;

			uint32_t alphaRef : 16; // half float
		};
	};
};


struct InstanceRenderConstants
{
	XMFLOAT4X4					mWorldTransform = MathHelper::Identity4x4();
	XMFLOAT4X4					mTexTransform = MathHelper::Identity4x4();
	int							mMaterialIndex;
	int							mPAD1;
	int							mPAD2;
	int							mPAD3;
};


struct MainConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	XMFLOAT4 gAmbientLight = { 0.0f, 0.0f, 0.0f ,0.0f };

	// 索引[0，NUM_DIR_LIGHTS）是方向灯；
	 //索引[NUM_DIR_LIGHTS，NUM_DIR_LIGHTS + NUM_POINT_LIGHTS）是点光源；
	 //索引[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS，NUM_DIR_LIGHTS + NUM_POINT_LIGHT + NUM_SPOT_LIGHTS）
	 //是聚光灯，每个对象最多可使用MaxLights。
	Light Lights[MaxLights];

	//ShadowMap Index
	int mShadowMapIndex = 0;
	int mSkyCubeIndex = 0;
	int mBakingDiffuseCubeIndex = 0;
	int gBakingPreFilteredEnvIndex = 0;
	int gBakingIntegrationBRDFIndex = 0;
	float IBLRange = 4.0f;
	float IBLBias = 0.0f;
	int mPad03 = 0;
};

struct TerrainConstants
{
	XMFLOAT4 gWorldFrustumPlanes[6];

	float gTessellationScale;
	float gTexelCellSpaceU;
	float gTexelCellSpaceV;
	float gWorldCellSpace;

	int gHeightMapIndex = 0;
	int gBlendMapIndex = 0;
	int gRGBNoiseMapIndex = 0;
};



struct CubeMapConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	int mSkyCubeIndex = 0;
	float mRoughness = 0;
	int mPad02 = 0;
	int mPad03 = 0;
};


struct GlobleSRVIndex
{
	static int gSkyCubeSRVIndex;
	static int gBakingDiffuseCubeIndex;
	static int gBakingPreFilteredEnvIndex;
	static int gBakingIntegrationBRDFIndex;
};
