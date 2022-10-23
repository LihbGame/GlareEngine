// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "PBRLighting.hlsli"



struct MaterialData
{
	float4   DiffuseAlbedo;
	float3   FresnelR0;
	uint     RoughnessMapIndex;
	float4x4 MatTransform;
	uint     DiffuseMapIndex;
    uint     NormalMapIndex;
    uint     MetallicMapIndex;
    uint     AOMapIndex;
    uint     HeightMapIndex;
    float    height_scale;
    uint     MaterialPad1;
    uint     MaterialPad2;
};


struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    uint     MaterialIndex;
    uint     height_scale;
    uint     InstPad1;
    uint     InstPad2;
};


TextureCube gCubeMap : register(t0);
//�������飬����ɫ��ģ��5.1+֧�֡� ��Texture2DArray��ͬ��
//�������е�������Ծ��в�ͬ�Ĵ�С�͸�ʽ��ʹ��������������
Texture2D gSRVMap[91] : register(t1);
//����space1������������鲻������Щ��Դ�ص���
//�������齫ռ��space0�еļĴ���t0��t1��...��t3��
StructuredBuffer<MaterialData> gMaterialData : register(t1, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

//ÿ֡�仯��ÿ����Ⱦ��ĳ������ݡ�
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	float4x4 gTexTransform;
	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

// ÿ֡�������ݡ�
cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // ����[0��NUM_DIR_LIGHTS���Ƿ���ƣ�
     //����[NUM_DIR_LIGHTS��NUM_DIR_LIGHTS + NUM_POINT_LIGHTS���ǵ��Դ��
     //����[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS��NUM_DIR_LIGHTS + NUM_POINT_LIGHT + NUM_SPOT_LIGHTS��
     //�Ǿ۹�ƣ�ÿ����������ʹ��MaxLights��
    Light gLights[MaxLights];

   int     gShadowMapIndex;
   int     gWaterReflectionMapIndex;
   int     gWaterRefractionMapIndex;
   int     gWaterDumpWaveIndex;

   float4 gFogColor;
   float  gFogStart;
   float  gFogRange;
   int gFogEnabled;
   int gShadowEnabled;
};

#ifdef SKINNED
cbuffer cbSkinned : register(b2)
{
   float4x4 gBoneTransforms[96];
};
#endif

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to model space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToModelSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    //from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

    // Build TBN basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}


float3 WorldSpaceToTBN(float3 WorldSpaceVector, float3 unitNormalW, float3 tangentW)
{

    // Build TBN basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);
    float3 TBNSpaceVector = mul(WorldSpaceVector, transpose(TBN));

    return TBNSpaceVector;
}


//�Ӳ��ڵ�ӳ��
float2 ParallaxMapping(uint HeightMapIndex,float2 texCoords, float3 viewDir,float height_scale)
{
    // number of depth layers
    const float minLayers = 10;
    const float maxLayers = 20;
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0, 0.0, 1.0), viewDir)));
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    float2 P = viewDir.xy * height_scale;
    float2 deltaTexCoords = P / numLayers;

    // get initial values
    float2  currentTexCoords = texCoords;
    float currentDepthMapValue =1-gSRVMap[HeightMapIndex].SampleLevel(gsamLinearWrap, currentTexCoords,0).r;
    
    while (currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = 1-gSRVMap[HeightMapIndex].SampleLevel(gsamLinearWrap, currentTexCoords,0).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;
    }

    // -- parallax occlusion mapping interpolation from here on
    // get texture coordinates before collision (reverse operations)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1-gSRVMap[HeightMapIndex].SampleLevel(gsamLinearWrap, prevTexCoords, 0).r - currentLayerDepth + layerDepth;

    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}



//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gSRVMap[48].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gSRVMap[gShadowMapIndex].SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }

    return percentLit / 9.0f;
}


// �������ȫλ��ƽ��ĺ���(����ռ�),�򷵻�true��
bool AABBBehindPlaneTest(float3 center, float3 extents, float4 plane)
{
    float3 n = abs(plane.xyz);

    // This is always positive.
    float r = dot(extents, n);
    //�����ĵ㵽ƽ����������롣
    float s = dot(float4(center, 1.0f), plane);

    //���������ĵ���ƽ��������e����������������sΪ����
    //��Ϊ����ƽ����棩�������ȫλ��ƽ��ĸ���ռ��С�
    return (s + r) < 0.0f;
}


//����ÿ���ȫλ��ƽ��ͷ��֮�⣬����true��
bool AABBOutsideFrustumTest(float3 center, float3 extents, float4 frustumPlanes[6])
{
    for (int i = 0; i < 6; ++i)
    {
        // ���������ȫλ���κ�һ����׶ƽ��ĺ��棬��ô��������׶֮�⡣
        if (AABBBehindPlaneTest(center, extents, frustumPlanes[i]))
        {
            return true;
        }
    }
    return false;
}