#ifndef COMMON_RESOURCE
#define COMMON_RESOURCE

#define MAX2DSRVSIZE        1024
#define MAXCUBESRVSIZE      32
#define MAXPBRSRVSIZE       10
#define COMMONSRVSIZE       10

// Shadow map related variables
#define NUM_SAMPLES 20
#define BLOCKER_SEARCH_NUM_SAMPLES NUM_SAMPLES
#define PCF_NUM_SAMPLES NUM_SAMPLES
#define NUM_RINGS 10

// Numeric constants
static const float3 DielectricSpecular = float3(0.04, 0.04, 0.04);

#define MAX_DIR_LIGHTS 16
#define NUM_DIR_LIGHTS 3

#define EPS             1e-5
#define PI              3.141592653589793
#define PI2             6.283185307179586
#define FLT_MIN         1.175494351e-38F        // min positive value
#define FLT_MAX         3.402823466e+38F        // max value

struct DirectionalLight
{
    float3      Strength;
    int         Pad1;
    float3      Direction;
    int         Pad2;
};

struct SurfaceProperties
{
    float3  N;           //surface normal
    float3  V;           //normalized view vector
    float3  c_diff;
    float3  c_spec;      //The F0 reflectance value - 0.04 for non-metals, or RGB for metals.
    float3  worldPos;
    float   roughness;
    float   alpha;        // roughness squared
    float   alphaSqr;     // alpha squared
    float   NdotV;
    float   ShadowFactor;
    float   ao;
};

struct LightProperties
{
    float3  L;           //normalized direction to light
    float3  H;
    float   NdotL;
    float   LdotH;
    float   NdotH;

    float3  Strength;
    float3  Direction;
    float3  LightPosition;
};


struct Material
{
    float4  DiffuseAlbedo;
    float3  FresnelR0;
    float   Roughness;
    float   metallic;
    float   AO;
};



//Constant data per frame.
cbuffer MainPass : register(b0)
{
    float4x4    gView;
    float4x4    gInvView;
    float4x4    gProj;
    float4x4    gInvProj;
    float4x4    gViewProj;
    float4x4    gInvViewProj;
    float4x4    gShadowTransform;
    float3      gEyePosW;
    float       cbPerObjectPad1;
    float2      gRenderTargetSize;
    float2      gInvRenderTargetSize;
    float       gNearZ;
    float       gFarZ;
    float       gTotalTime;
    float       gDeltaTime;
    float4      gAmbientLight;

    DirectionalLight gLights[MAX_DIR_LIGHTS];

    float4      gInvTileDimension;
    uint4       gTileCount;
    uint4       gFirstLightIndex;

    int         gShadowMapIndex;
    int         gSkyCubeIndex;
    int         gBakingDiffuseCubeIndex;
    int         gBakingPreFilteredEnvIndex;
    int         gBakingIntegrationBRDFIndex;
    float       IBLRange;
    float       IBLBias;
    int         pad0;

    int         gDirectionalLightsCount;
    int         gIsIndoorScene;
    int         gIsRenderTiledBaseLighting;
};


struct InstanceData
{
    float4x4        World;
    float4x4        TexTransform;
    uint            MaterialIndex;
    uint            mPAD1;
    uint            mPAD2;
    uint            mPAD3;
};


struct MaterialData
{
    float4          mDiffuseAlbedo;
    float3          mFresnelR0;
    float           mHeightScale;
    float4x4        mMatTransform;
    uint            mRoughnessMapIndex;
    uint            mDiffuseMapIndex;
    uint            mNormalMapIndex;
    uint            mMetallicMapIndex;
    uint            mAOMapIndex;
    uint            mHeightMapIndex;
};

SamplerState gMaterialSampler[10]       : register(s0);

//static sampler
SamplerState gSamplerLinearWrap         : register(s10);
SamplerState gSamplerAnisoWrap          : register(s11);
SamplerState gSamplerLinearClamp        : register(s13);
SamplerState gSamplerVolumeWrap         : register(s14);
SamplerState gSamplerPointClamp         : register(s15);
SamplerState gSamplerPointBorder        : register(s16);
SamplerState gSamplerLinearBorder       : register(s17);
SamplerComparisonState gSamplerShadow   : register(s12);

//Texture array, only supported for shader model 5.1+. Unlike Texture2DArray,
//Textures in this array can have different sizes and formats, making it more flexible than texture arrays.

TextureCube gCubeMaps[MAXCUBESRVSIZE]    : register(t20);
Texture2D   gSRVMap[MAX2DSRVSIZE]        : register(t52);


StructuredBuffer<MaterialData> gMaterialData : register(t1, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t0, space1);

//Terrain
#if defined(TERRAIN)

#define NUM_CONTROL_POINTS 4

struct VertexIn
{
    float3 PosL     : POSITION;
    float2 Tex      : TEXCOORD;
    float2 BoundsY  : BoundY;
};

struct VertexOut
{
    float3 PosW     : POSITION;
    float2 Tex      : TEXCOORD;
    float2 BoundsY  : BoundY;
};

struct PatchTess
{
    float EdgeTess[4]   : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
    float3 PosW     : POSITION;
    float2 Tex      : TEXCOORD;
};

cbuffer TerrainCBPass : register(b1)
{
    float4      gWorldFrustumPlanes[6];

    float       gTessellationScale;
    float       gTexelCellSpaceU;
    float       gTexelCellSpaceV;
    float       gWorldCellSpace;

    int         gHeightMapIndex;
    int         gBlendMapIndex;

    float       gMinDist;
    float       gMaxDist;
    float       gMinTess;
    float       gMaxTess;
};
#endif


//Position
struct PosVSOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION0;
};

//Tex
struct TexVSOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEXCOORD0;
};


//PosNormalTangentTexc
struct PosNorTanTexIn
{
    float3 PosL         : POSITION;
    float3 NormalL      : NORMAL;
    float3 TangentL     : TANGENT;
    float2 TexC         : TEXCOORD;
};

struct PosNorTanTexOut
{
    float4 PosH         : SV_POSITION;
    float4 ShadowPosH   : POSITION0;
    float3 PosW         : POSITION1;
    float3 NormalW      : NORMAL;
    float3 TangentW     : TANGENT;
    float2 TexC         : TEXCOORD;
    uint MatIndex       : MATINDEX;
};

struct AABB
{
    float3 Center;
    float3 Extend;
};


struct Plane
{
    float3 N;   // Plane normal.
    float  d;   // Distance to origin.
};

struct Sphere
{
    float3 c;   // Center point.
    float  r;   // Radius.
};

struct Cone
{
    float3 T;   // Cone tip.
    float  h;   // Height of the cone.
    float3 d;   // Direction of the cone.
    float  r;   // bottom radius of the cone.
};


inline bool Is_Saturated(float a) { return a == saturate(a); }
inline bool Is_Saturated(float2 a) { return all(a == saturate(a)); }
inline bool Is_Saturated(float3 a) { return all(a == saturate(a)); }
inline bool Is_Saturated(float4 a) { return all(a == saturate(a)); }


// This is for light culling
// Four planes of a view frustum (in view space).
// The planes are:
//  * Left,
//  * Right,
//  * Top,
//  * Bottom.
// The back and/or front planes can be computed from depth values in the 
// light culling compute shader.
struct Frustum
{
    Plane planes[4];   // left, right, top, bottom frustum planes.
};

//AABB and sphere intersection test
bool IntersectionAABBvsSphere(Sphere sphere,AABB aabb)
{
    float3 delta = max(0, abs(aabb.Center - sphere.c) - aabb.Extend);
    float distSq = dot(delta, delta);
    return distSq <= sphere.r * sphere.r;
}

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to model space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToModelSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
   // Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
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


//视差遮挡映射
float2 ParallaxMapping(uint HeightMapIndex, float2 texCoords, float3 viewDir, float height_scale)
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
    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = 1 - gSRVMap[HeightMapIndex].SampleLevel(gSamplerLinearWrap, currentTexCoords, 0).r;
    
    while (currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = 1 - gSRVMap[HeightMapIndex].SampleLevel(gSamplerLinearWrap, currentTexCoords, 0).r;
        // get depth of next layer
        currentLayerDepth += layerDepth;
    }

    // -- parallax occlusion mapping interpolation from here on
    // get texture coordinates before collision (reverse operations)
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1 - gSRVMap[HeightMapIndex].SampleLevel(gSamplerLinearWrap, prevTexCoords, 0).r - currentLayerDepth + layerDepth;

    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}



// 如果框完全位于平面的后面(负半空间),则返回true。
bool AABBBehindPlaneTest(float3 center, float3 extents, float4 plane)
{
    float3 n = abs(plane.xyz);

    // This is always positive.
    float r = dot(extents, n);
    //从中心点到平面的正负距离。
    float s = dot(float4(center, 1.0f), plane);

    //如果框的中心点在平面后面等于e或更大（在这种情况下s为负，
    //因为它在平面后面），则框完全位于平面的负半空间中。
    return (s + r) < 0.0f;
}


//如果该框完全位于平截头体之外，返回true。
bool AABBOutsideFrustumTest(float3 center, float3 extents, float4 frustumPlanes[6])
{
    for (int i = 0; i < 6; ++i)
    {
        // 如果盒子完全位于任何一个视锥平面的后面，那么它就在视锥之外。
        if (AABBBehindPlaneTest(center, extents, frustumPlanes[i]))
        {
            return true;
        }
    }
    return false;
}


///Random Function

//One Dimensional (0-1)

float random11(float x) 
{
    return frac(sin(x) * 10000.0f);
}

float2 random21(float p)
{
    float3 p3 = frac(float3(p, p, p) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float3 random31(float p)
{
    float3 p3 = frac(float3(p, p, p) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xxy + p3.yzz) * p3.zyx);
}

//Two Dimensional (0-1)

float random12(float2 uv)
{
    const float a = 12.9898f, b = 78.233f, c = 43758.5453f;
    float dt = dot(uv.xy, float2(a, b)), sn = fmod(dt, PI);
    return frac(sin(sn) * c);
}

float2 random22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float3 random32(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return frac((p3.xxy + p3.yzz) * p3.zyx);
}


//Three Dimensional (0-1)

float random13(float3 p)  // replace this by something better
{
    p = 50.0 * frac(p * 0.3183099f + float3(0.71, 0.113, 0.419));
    return -1.0 + 2.0 * frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}


float2 random23(float3 p3)
{
    p3 = frac(p3 * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float3 random33(float3 p3)
{
    p3 = frac(p3 * float3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return frac((p3.xxy + p3.yxx) * p3.zyx);
}


///Noise Function

//2D value noise
float ValueNoise(in float2 st)
{
    float2 i = floor(st);
    float2 f = frac(st);

    // Four corners in 2D of a tile
    float a = random12(i);
    float b = random12(i + float2(1.0, 0.0));
    float c = random12(i + float2(0.0, 1.0));
    float d = random12(i + float2(1.0, 1.0));

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    float2 u = f * f * (3.0 - 2.0 * f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return lerp(a, b, u.x) +
        (c - a) * u.y * (1.0 - u.x) +
        (d - b) * u.x * u.y;
}

//2D value noise (in x) and its derivatives (in yz)
float3 ValueNoised(in float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

#if 1
    // quintic interpolation
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
#else
    // cubic interpolation
    float2 u = f * f * (3.0 - 2.0 * f);
    float2 du = 6.0 * f * (1.0 - f);
#endif    

    float va = random12(i + float2(0.0, 0.0));
    float vb = random12(i + float2(1.0, 0.0));
    float vc = random12(i + float2(0.0, 1.0));
    float vd = random12(i + float2(1.0, 1.0));

    float k0 = va;
    float k1 = vb - va;
    float k2 = vc - va;
    float k4 = va - vb - vc + vd;

    return float3(va + k1 * u.x + k2 * u.y + k4 * u.x * u.y, // value
        du * (u.yx * k4 + float2(vb, vc) - va));     // derivative                
}

//3D Value Noise
float ValueNoise(in float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(lerp(random13(i + float3(0, 0, 0)),random13(i + float3(1, 0, 0)), f.x),
           lerp(random13(i + float3(0, 1, 0)),random13(i + float3(1, 1, 0)), f.x), f.y),
           lerp(lerp(random13(i + float3(0, 0, 1)),random13(i + float3(1, 0, 1)), f.x),
           lerp(random13(i + float3(0, 1, 1)),random13(i + float3(1, 1, 1)), f.x), f.y), f.z);
}

//3D Value Noise (in x) and its derivatives (in yzw)
float4 ValueNoised(in float3 x)
{
    float3 i = floor(x);
    float3 w = frac(x);

#if 1
    // quintic interpolation
    float3 u = w * w * w * (w * (w * 6.0 - 15.0) + 10.0);
    float3 du = 30.0 * w * w * (w * (w - 2.0) + 1.0);
#else
    // cubic interpolation
    float3 u = w * w * (3.0 - 2.0 * w);
    float3 du = 6.0 * w * (1.0 - w);
#endif    


    float a = random13(i + float3(0.0, 0.0, 0.0));
    float b = random13(i + float3(1.0, 0.0, 0.0));
    float c = random13(i + float3(0.0, 1.0, 0.0));
    float d = random13(i + float3(1.0, 1.0, 0.0));
    float e = random13(i + float3(0.0, 0.0, 1.0));
    float f = random13(i + float3(1.0, 0.0, 1.0));
    float g = random13(i + float3(0.0, 1.0, 1.0));
    float h = random13(i + float3(1.0, 1.0, 1.0));

    float k0 = a;
    float k1 = b - a;
    float k2 = c - a;
    float k3 = e - a;
    float k4 = a - b - c + d;
    float k5 = a - c - e + g;
    float k6 = a - b - e + f;
    float k7 = -a + b + c - d + e - f - g + h;

    return float4(k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z,
        du * float3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
            k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
            k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y));
}

//2D Gradient noise
float GradientNoise(in float2 p)
{
    float2 i = float2(floor(p));
    float2 f = frac(p);

    float2 u = f * f * (3.0 - 2.0 * f); // feel free to replace by a quintic smoothstep instead

    return lerp(lerp(dot(random22(i + float2(0, 0)), f - float2(0.0, 0.0)),
        dot(random22(i + float2(1, 0)), f - float2(1.0, 0.0)), u.x),
        lerp(dot(random22(i + float2(0, 1)), f - float2(0.0, 1.0)),
            dot(random22(i + float2(1, 1)), f - float2(1.0, 1.0)), u.x), u.y);
}


//2D Gradient noise (in .x)  and its derivatives (in .yz)
float3 GradientNoised(in float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);

    float2 ga = random22(i + float2(0.0, 0.0));
    float2 gb = random22(i + float2(1.0, 0.0));
    float2 gc = random22(i + float2(0.0, 1.0));
    float2 gd = random22(i + float2(1.0, 1.0));

    float va = dot(ga, f - float2(0.0, 0.0));
    float vb = dot(gb, f - float2(1.0, 0.0));
    float vc = dot(gc, f - float2(0.0, 1.0));
    float vd = dot(gd, f - float2(1.0, 1.0));

    return float3(va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * (va - vb - vc + vd),   // value
        ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) +  // derivatives
        du * (u.yx * (va - vb - vc + vd) + float2(vb, vc) - va));
}

//3D Gradient noise
float GradientNoise(in float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);

    float3 u = f * f * (3.0 - 2.0 * f);

    return lerp(lerp(lerp(dot(random33(i + float3(0.0, 0.0, 0.0)), f - float3(0.0, 0.0, 0.0)), dot(random33(i + float3(1.0, 0.0, 0.0)), f - float3(1.0, 0.0, 0.0)), u.x),
           lerp(dot(random33(i + float3(0.0, 1.0, 0.0)), f - float3(0.0, 1.0, 0.0)),dot(random33(i + float3(1.0, 1.0, 0.0)), f - float3(1.0, 1.0, 0.0)), u.x), u.y),
           lerp(lerp(dot(random33(i + float3(0.0, 0.0, 1.0)), f - float3(0.0, 0.0, 1.0)),dot(random33(i + float3(1.0, 0.0, 1.0)), f - float3(1.0, 0.0, 1.0)), u.x),
           lerp(dot(random33(i + float3(0.0, 1.0, 1.0)), f - float3(0.0, 1.0, 1.0)),dot(random33(i + float3(1.0, 1.0, 1.0)), f - float3(1.0, 1.0, 1.0)), u.x), u.y), u.z);
}

//3D Gradient noise (in .x)  and its derivatives (in .yzw)
float4 GradientNoised(in float3 x)
{
    // grid
    float3 i = floor(x);
    float3 w = frac(x);

#if 1
    // quintic interpolant
    float3 u = w * w * w * (w * (w * 6.0 - 15.0) + 10.0);
    float3 du = 30.0 * w * w * (w * (w - 2.0) + 1.0);
#else
    // cubic interpolant
    float3 u = w * w * (3.0 - 2.0 * w);
    float3 du = 6.0 * w * (1.0 - w);
#endif    

    // gradients
    float3 ga = random33(i + float3(0.0, 0.0, 0.0));
    float3 gb = random33(i + float3(1.0, 0.0, 0.0));
    float3 gc = random33(i + float3(0.0, 1.0, 0.0));
    float3 gd = random33(i + float3(1.0, 1.0, 0.0));
    float3 ge = random33(i + float3(0.0, 0.0, 1.0));
    float3 gf = random33(i + float3(1.0, 0.0, 1.0));
    float3 gg = random33(i + float3(0.0, 1.0, 1.0));
    float3 gh = random33(i + float3(1.0, 1.0, 1.0));

    // projections
    float va = dot(ga, w - float3(0.0, 0.0, 0.0));
    float vb = dot(gb, w - float3(1.0, 0.0, 0.0));
    float vc = dot(gc, w - float3(0.0, 1.0, 0.0));
    float vd = dot(gd, w - float3(1.0, 1.0, 0.0));
    float ve = dot(ge, w - float3(0.0, 0.0, 1.0));
    float vf = dot(gf, w - float3(1.0, 0.0, 1.0));
    float vg = dot(gg, w - float3(0.0, 1.0, 1.0));
    float vh = dot(gh, w - float3(1.0, 1.0, 1.0));

    // interpolations
    return float4(va + u.x * (vb - va) + u.y * (vc - va) + u.z * (ve - va) + u.x * u.y * (va - vb - vc + vd) + u.y * u.z * (va - vc - ve + vg) + u.z * u.x * (va - vb - ve + vf) + (-va + vb + vc - vd + ve - vf - vg + vh) * u.x * u.y * u.z,    // value
        ga + u.x * (gb - ga) + u.y * (gc - ga) + u.z * (ge - ga) + u.x * u.y * (ga - gb - gc + gd) + u.y * u.z * (ga - gc - ge + gg) + u.z * u.x * (ga - gb - ge + gf) + (-ga + gb + gc - gd + ge - gf - gg + gh) * u.x * u.y * u.z +   // derivatives
        du * (float3(vb, vc, ve) - va + u.yzx * float3(va - vb - vc + vd, va - vc - ve + vg, va - vb - ve + vf) + u.zxy * float3(va - vb - ve + vf, va - vb - vc + vd, va - vc - ve + vg) + u.yzx * u.zxy * (-va + vb + vc - vd + ve - vf - vg + vh)));
}



//Poisson Disk
struct DiskSamples
{
    float2 Samples[NUM_SAMPLES];
};


DiskSamples poissonDiskSamples(const in float2 randomSeed) {

    float ANGLE_STEP = PI2 * float(NUM_RINGS) / float(NUM_SAMPLES);
    float INV_NUM_SAMPLES = 1.0 / float(NUM_SAMPLES);
    
    float angle = random12(randomSeed) * PI2;
    float radius = INV_NUM_SAMPLES;
    float radiusStep = radius;

    DiskSamples poissonDisk;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        poissonDisk.Samples[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.75);
        radius += radiusStep;
        angle += ANGLE_STEP;
    }
    return poissonDisk;
}

DiskSamples uniformDiskSamples(const in float2 randomSeed) {

    float randNum = random12(randomSeed);
    float sampleX = random11(randNum);
    float sampleY = random11(sampleX);

    float angle = sampleX * PI2;
    float radius = sqrt(sampleY);
    DiskSamples poissonDisk;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        poissonDisk.Samples[i] = float2(radius * cos(angle), radius * sin(angle));

        sampleX = random11(sampleY);
        sampleY = random11(sampleX);

        angle = sampleX * PI2;
        radius = sqrt(sampleY);
    }
    return poissonDisk;
}


#define NUM_OCTAVES 5

float FBM(in float2 st)
{
    float v = 0.0;
    float a = 0.5;
    float2 shift = float2(100.0f, 100.0f);
    // Rotate to reduce axial bias
    float2x2 rot = { cos(0.5f), sin(0.5f),-sin(0.5f), cos(0.5f) };

    for (int i = 0; i < NUM_OCTAVES; ++i) 
    {
        v += a * ValueNoise(st);
        st = mul(st, rot) * 2.0 + shift;
        a *= 0.5f;
    }
    return v;
}

float3 FBM_Liquid(in float2 FragCoord)
{
    float2 st = FragCoord / float2(384, 384) * 4.0f;

    float3 color = float3(0, 0, 0);

    float2 q = float2(0, 0);
    q.x = FBM(st);
    q.y = FBM(st + float2(1.0f, 1.0f));

    float2 r = float2(0, 0);
    r.x = FBM(st + 1.0 * q + float2(1.7, 9.2) + 0.45* gTotalTime);
    r.y = FBM(st + 1.0 * q + float2(8.3, 2.8) + 0.826 * gTotalTime);

    float f = FBM(st + r);

    color = lerp(float3(0.101961f, 0.619608f, 0.666667f),
        float3(0.666667f, 0.666667f, 0.498039f),
        clamp((f * f) * 4.0f, 0.0f, 1.0f));

    color = lerp(color,
        float3(0, 0, 0.164706f),
        clamp(length(q), 0.0f, 1.0f));

    color = lerp(color,
        float3(0.666667f, 1, 1),
        clamp(length(r.x), 0.0f, 1.0f));

    return float3((f * f * f + .6f * f * f + .5f * f) * color);
}


// 2D array index to flattened 1D array index
inline uint flatten2D(uint2 coord, uint2 dim)
{
    return coord.x + coord.y * dim.x;
}
// flattened array index to 2D array index
inline uint2 unflatten2D(uint idx, uint2 dim)
{
    return uint2(idx % dim.x, idx / dim.x);
}

// 3D array index to flattened 1D array index
inline uint flatten3D(uint3 coord, uint3 dim)
{
    return (coord.z * dim.x * dim.y) + (coord.y * dim.x) + coord.x;
}
// flattened array index to 3D array index
inline uint3 unflatten3D(uint idx, uint3 dim)
{
    const uint z = idx / (dim.x * dim.y);
    idx -= (z * dim.x * dim.y);
    const uint y = idx / dim.x;
    const uint x = idx % dim.x;
    return  uint3(x, y, z);
}

// Reconstructs world-space position from depth buffer
//	uv		: screen space coordinate in [0, 1] range
//	z		: depth value at current pixel
//	InvVP	: Inverse of the View-Projection matrix that was used to generate the depth value
inline float3 Reconstruct_Position(in float2 uv, in float z, in float4x4 inverse_view_projection)
{
    float x = uv.x * 2 - 1;
    float y = (1 - uv.y) * 2 - 1;
    float4 position_cvv = float4(x, y, z, 1);
    float4 position = mul(position_cvv, inverse_view_projection);
    return position.xyz / position.w;
}


float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// A uniform 2D random generator for hemisphere sampling: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
//	i	: iteration index
//	N	: number of iterations in total
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// Point on hemisphere with uniform distribution
//	u, v : in range [0, 1]
float3 HemispherePoint_Uniform(float u, float v) 
{
    float phi = v * 2 * PI;
    float cosTheta = 1 - u;
    float sinTheta = sqrt(1 - cosTheta * cosTheta);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Point on hemisphere with cosine-weighted distribution
//	u, v : in range [0, 1]
float3 HemispherePoint_Cos(float u, float v) 
{
    float phi = v * 2 * PI;
    float cosTheta = sqrt(1 - u);
    float sinTheta = sqrt(1 - cosTheta * cosTheta);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Random number generator based on: https://github.com/diharaw/helios/blob/master/src/engine/shader/random.glsl
struct RandomNumberGenerator
{
    uint2 s; // state

    // xoroshiro64* random number generator.
    // http://prng.di.unimi.it/xoroshiro64star.c
    uint rotl(uint x, uint k)
    {
        return (x << k) | (x >> (32 - k));
    }
    // Xoroshiro64* RNG
    uint next()
    {
        uint result = s.x * 0x9e3779bb;

        s.y ^= s.x;
        s.x = rotl(s.x, 26) ^ s.y ^ (s.y << 9);
        s.y = rotl(s.y, 13);

        return result;
    }
    // Thomas Wang 32-bit hash.
    // http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
    uint hash(uint seed)
    {
        seed = (seed ^ 61) ^ (seed >> 16);
        seed *= 9;
        seed = seed ^ (seed >> 4);
        seed *= 0x27d4eb2d;
        seed = seed ^ (seed >> 15);
        return seed;
    }

    void init(uint2 id, uint frameIndex)
    {
        uint s0 = (id.x << 16) | id.y;
        uint s1 = frameIndex;
        s.x = hash(s0);
        s.y = hash(s1);
        next();
    }
    float next_float()
    {
        uint u = 0x3f800000 | (next() >> 9);
        return asfloat(u) - 1.0;
    }
    uint next_uint(uint nmax)
    {
        float f = next_float();
        return uint(floor(f * nmax));
    }
    float2 next_float2()
    {
        return float2(next_float(), next_float());
    }
    float3 next_float3()
    {
        return float3(next_float(), next_float(), next_float());
    }
};



// Maps standard viewport UV to screen position.
float2 ViewportUVToScreenPos(float2 ViewportUV)
{
    return float2(2 * ViewportUV.x - 1, 1 - 2 * ViewportUV.y);
}

float2 ScreenPosToViewportUV(float2 ScreenPos)
{
    return float2(0.5 + 0.5 * ScreenPos.x, 0.5 - 0.5 * ScreenPos.y);
}

#define BICUBIC_CATMULL_ROM_SAMPLES 5

struct CatmullRomSamples
{
	// Constant number of samples (BICUBIC_CATMULL_ROM_SAMPLES)
    uint Count;

	// Constant sign of the UV direction from main UV sampling location.
    int2 UVDir[BICUBIC_CATMULL_ROM_SAMPLES];

	// Bilinear sampling UV coordinates of the samples
    float2 UV[BICUBIC_CATMULL_ROM_SAMPLES];

	// Weights of the samples
    float Weight[BICUBIC_CATMULL_ROM_SAMPLES];

	// Final multiplier (it is faster to multiply 3 RGB values than reweights the 5 weights)
    float FinalMultiplier;
};

void Bicubic2DCatmullRom(in float2 UV, in float2 Size, in float2 InvSize, out float2 Sample[3], out float2 Weight[3])
{
    UV *= Size;

    float2 tc = floor(UV - 0.5) + 0.5;
    float2 f = UV - tc;
    float2 f2 = f * f;
    float2 f3 = f2 * f;

    float2 w0 = f2 - 0.5 * (f3 + f);
    float2 w1 = 1.5 * f3 - 2.5 * f2 + 1;
    float2 w3 = 0.5 * (f3 - f2);
    float2 w2 = 1 - w0 - w1 - w3;

    Weight[0] = w0;
    Weight[1] = w1 + w2;
    Weight[2] = w3;

    Sample[0] = tc - 1;
    Sample[1] = tc + w2 * rcp(Weight[1]);
    Sample[2] = tc + 2;

    Sample[0] *= InvSize;
    Sample[1] *= InvSize;
    Sample[2] *= InvSize;
}

CatmullRomSamples GetBicubic2DCatmullRomSamples(float2 UV, float2 Size, in float2 InvSize)
{
    CatmullRomSamples Samples;
    Samples.Count = BICUBIC_CATMULL_ROM_SAMPLES;

    float2 Weight[3];
    float2 Sample[3];
    Bicubic2DCatmullRom(UV, Size, InvSize, Sample, Weight);

	// Optimized by removing corner samples
    Samples.UV[0] = float2(Sample[1].x, Sample[0].y);
    Samples.UV[1] = float2(Sample[0].x, Sample[1].y);
    Samples.UV[2] = float2(Sample[1].x, Sample[1].y);
    Samples.UV[3] = float2(Sample[2].x, Sample[1].y);
    Samples.UV[4] = float2(Sample[1].x, Sample[2].y);

    Samples.Weight[0] = Weight[1].x * Weight[0].y;
    Samples.Weight[1] = Weight[0].x * Weight[1].y;
    Samples.Weight[2] = Weight[1].x * Weight[1].y;
    Samples.Weight[3] = Weight[2].x * Weight[1].y;
    Samples.Weight[4] = Weight[1].x * Weight[2].y;

    Samples.UVDir[0] = int2(0, -1);
    Samples.UVDir[1] = int2(-1, 0);
    Samples.UVDir[2] = int2(0, 0);
    Samples.UVDir[3] = int2(1, 0);
    Samples.UVDir[4] = int2(0, 1);

	// Reweight after removing the corners
    float CornerWeights;
    CornerWeights = Samples.Weight[0];
    CornerWeights += Samples.Weight[1];
    CornerWeights += Samples.Weight[2];
    CornerWeights += Samples.Weight[3];
    CornerWeights += Samples.Weight[4];
    Samples.FinalMultiplier = 1 / CornerWeights;

    return Samples;
}

// Convert clip space coordinates to view space
float4 ClipToView(float4 clip, float4x4 InvProj)
{
    // View space position.
    float4 view = mul(InvProj, clip);
    // Perspecitive projection.
    view = view / view.w;

    return view;
}

// Convert screen space coordinates to view space.
float4 ScreenToView(float4 screen, float2 Viewport, float4x4 InvProj)
{
    screen.xy = min(screen.xy, uint2(Viewport.x, Viewport.y)); // avoid loading from outside the texture, it messes up the min-max depth!
    // Convert to normalized texture coordinates
    float2 texCoord = screen.xy / float2(Viewport.x, Viewport.y);

    // Convert to clip space
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);

    return ClipToView(clip, InvProj);
}
#endif