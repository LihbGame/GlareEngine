#include "../Misc/CommonResource.hlsli"
#include "VelocityPacking.hlsli"
#include "../Misc/ShaderUtility.hlsli"

Texture2D DepthTexture		: register(t0);
Texture2D PreDepthTexture	: register(t1);
Texture2D InputSceneColor	: register(t2);
Texture2D HistoryColor		: register(t3);

RWTexture2D<float4> OutTexture : register(u0);

Texture2D<Packed_Velocity_Type> VelocityBuffer : register(t4); // Full resolution motion vectors

SamplerState BiLinearClampSampler	: register(s0);

cbuffer TAAConstantBuffer : register(b1)
{
    matrix CurrentToPrev;
    float4 OutputViewportSize;
    float4 InputViewSize;
	// Temporal jitter at the pixel.
    float2 TemporalJitterPixels;
    float2 ViewportJitter;
	//ue5 set 0.04 as default ,Low values cause blurriness and ghosting, high values fail to hide jittering
    float CurrentFrameWeight;
    float pad1;
	//CatmullRom Weight
    float SampleWeights[9];
    float PlusWeights[5];
}

#define THREADGROUP_SIZEX 8
#define THREADGROUP_SIZEY 8


//Qualities
#define TAA_QUALITY_LOW		0
#define TAA_QUALITY_MEDIUM	1
#define TAA_QUALITY_HIGH	2


///////////////////////////////// Clamping method for scene color /////////////////////////////////////

// Min max neighboorhing samples.
#define HISTORY_CLAMPING_BOX_MIN_MAX 0

// Variance computed from neighboorhing samples.
#define HISTORY_CLAMPING_BOX_VARIANCE 1

// Min max samples that are within distance from output pixel. 
#define HISTORY_CLAMPING_BOX_SAMPLE_DISTANCE 2


///////////////////////////////// Caching method for scene color /////////////////////////////////////

// Prefetches scene color into 10x10 LDS tile.
#define AA_SAMPLE_CACHE_METHOD_LDS 1


//// Payload of the history. History might still have addtional TAA internals//////////////////////////

// Only have RGB.
#define HISTORY_PAYLOAD_RGB 0

// Have RGB and translucency in alpha.
#define HISTORY_PAYLOAD_RGB_TRANSLUCENCY 1

// Have RGB and opacity in alpha.
#define HISTORY_PAYLOAD_RGB_OPACITY (HISTORY_PAYLOAD_RGB_TRANSLUCENCY)

//Quality setting
#define TAA_QUALITY TAA_QUALITY_HIGH

//Bicubic filter history
#define AA_BICUBIC 1
//Use dynamic motion
#define AA_DYNAMIC 1
//Tone map to kill fireflies
#define AA_TONE 1
//Use YCoCg
#define AA_YCOCG 1

//Upsample the output
#define AA_UPSAMPLE 0

#define UPSCALE_FACTOR 1.0

//Cross distance in pixels used in depth search X pattern.
#define AA_CROSS 2

#if TAA_QUALITY == TAA_QUALITY_LOW
//Always enable scene color filtering
#define AA_FILTERED 0

#elif TAA_QUALITY == TAA_QUALITY_MEDIUM
#define AA_FILTERED 1

#elif TAA_QUALITY == TAA_QUALITY_HIGH
#define AA_HISTORY_CLAMPING_BOX (HISTORY_CLAMPING_BOX_SAMPLE_DISTANCE)
#define AA_FILTERED 1
//Antighosting using dynamic mask
#define AA_DYNAMIC_ANTIGHOST 1
#define AA_SAMPLES 9

#endif

#ifndef AA_SAMPLES
	#define AA_SAMPLES 5
#endif


#define AA_CLAMP 1

static const int2 Offsets3x3[9] =
{
	int2(-1, -1),
	int2(0,  -1),
	int2(1,  -1),
	int2(-1,  0),
	int2(0,   0), // center
	int2(1,   0),
	int2(-1,  1),
	int2(0,   1),
	int2(1,   1),
};

//Indexes of the 3x3 square.
static const uint SquareIndexes3x3[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

//Indexes of the offsets to have shape.
static const uint PlusIndexes3x3[5] = { 1, 3, 4, 5, 7 };

//Number of neighbors.
static const uint NeighborsCount = 9;

RWTexture2D<float4> OutComputeTex;


float3 RGBToYCoCg(float3 RGB)
{
	float Y = dot(RGB, float3(1, 2, 1));
	float Co = dot(RGB, float3(2, 0, -2));
	float Cg = dot(RGB, float3(-1, 2, -1));

	float3 YCoCg = float3(Y, Co, Cg);
	return YCoCg;
}

float3 YCoCgToRGB(float3 YCoCg)
{
	float Y = YCoCg.x * 0.25;
	float Co = YCoCg.y * 0.25;
	float Cg = YCoCg.z * 0.25;

	float R = Y + Co - Cg;
	float G = Y + Cg;
	float B = Y - Co - Cg;

	float3 RGB = float3(R, G, B);
	return RGB;
}

// Faster but less accurate luma computation. 
// Luma includes a scaling by 4.
float Luma4(float3 Color)
{
	return (Color.g * 2.0) + (Color.r + Color.b);
}

// Optimized HDR weighting function.
float HdrWeight4(float3 Color, float Exposure)
{
	return rcp(Luma4(Color) * Exposure + 4.0);
}

float HdrWeightY(float Color, float Exposure)
{
	return rcp(Color * Exposure + 4.0);
}

float2 WeightedLerpFactors(float WeightA, float WeightB, float Blend)
{
	float BlendA = (1.0 - Blend) * WeightA;
	float BlendB = Blend * WeightB;
	float RcpBlend = rcp(BlendA + BlendB);
	BlendA *= RcpBlend;
	BlendB *= RcpBlend;
	return float2(BlendA, BlendB);
}

//Inaccurate but fast calculations
float ClipHistory(float3 History, float3 Filtered, float3 NeighborMin, float3 NeighborMax)
{
	float3 BoxMin = NeighborMin;
	float3 BoxMax = NeighborMax;

	float3 RayOrigin = History;
	float3 RayDir = Filtered - History;
	RayDir = abs(RayDir) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir;
	float3 InvRayDir = rcp(RayDir);

	float3 MinIntersect = (BoxMin - RayOrigin) * InvRayDir;
	float3 MaxIntersect = (BoxMax - RayOrigin) * InvRayDir;
	float3 EnterIntersect = min(MinIntersect, MaxIntersect);
	return max(max(EnterIntersect.x, EnterIntersect.y), EnterIntersect.z);
}


float4 MulPayload(in float4 Payload, in float x)
{
    Payload *= x;
	return Payload;
}

float4 AddPayload(in float4 Payload0, in float4 Payload1)
{
	Payload0 += Payload1;
	return Payload0;
}

float4 MinPayload(in float4 Payload0, in float4 Payload1)
{
	Payload0 = min(Payload0, Payload1);
	return Payload0;
}

float4 MaxPayload(in float4 Payload0, in float4 Payload1)
{
	Payload0 = max(Payload0, Payload1);
	return Payload0;
}

float4 MinPayload3(in float4 Payload0, in float4 Payload1, in float4 Payload2)
{
	Payload0 = min(min(Payload0, Payload1), Payload2);
	return Payload0;
}

float4 MaxPayload3(in float4 Payload0, in float4 Payload1, in float4 Payload2)
{
	Payload0 = max(max(Payload0, Payload1), Payload2);
	return Payload0;
}

struct TAAIntermediaryResult
{
	// The filtered input.
	float4	Filtered;

	// Temporal weight of the filtered input.
	float	FilteredTemporalWeight;
};


// Create intermediary result.
TAAIntermediaryResult CreateIntermediaryResult()
{
    TAAIntermediaryResult IntermediaryResult = (TAAIntermediaryResult) (0.0);

	IntermediaryResult.FilteredTemporalWeight = 1;
	return IntermediaryResult;
}

// Output pixel parameters. Should not be modified once setup.
struct TAAInputParameters
{
	// Compute shader dispatch params.
	uint2				GroupId;
	uint2				GroupThreadId;
	uint				GroupThreadIndex;

	// Position of the output pixel on screen.
	float2				ScreenPos;

	// Viewport UV of the output pixel.
	float2				ViewportUV;

	// Buffer UV of the nearest input pixel.
	float2				NearestBufferUV;

	// Whether this pixel should be responsive.
	float				bIsResponsiveAAPixel;

	// Frame exposure's scale.
	float				FrameExposureScale;
};



// Transformed scene color's data for a sample.
struct TAASceneColorSample
{
	// Transformed scene color and alpha channel.
	float4 Color;

	// HDR weight of the scene color sample.
	float HdrWeight;
};


// Transform RAW linear scene color RGB to TAA's working color space.
float4 TransformSceneColor(float4 RawLinearSceneColorRGBA)
{
#if AA_YCOCG
	return float4(RGBToYCoCg(RawLinearSceneColorRGBA.rgb), RawLinearSceneColorRGBA.a);
#endif
	return RawLinearSceneColorRGBA;
}

// Reciprocal of TransformSceneColor().
float4 TransformBackToRawLinearSceneColor(float4 SceneColor)
{
#if AA_YCOCG
	return float4(YCoCgToRGB(SceneColor.xyz), SceneColor.a);
#endif
	return SceneColor;
}

// Transform current frame's RAW scene color RGB to TAA's working color space.
float4 TransformCurrentFrameSceneColor(float4 RawSceneColorRGBA)
{
	return TransformSceneColor(RawSceneColorRGBA);
}

// Get the Luma4 of the sceneColor
float GetSceneColorLuma4(in float4 SceneColor)
{
#if AA_YCOCG
    return SceneColor.x;
#endif
	return Luma4(SceneColor.rgb);
}

// Get the HDR weight of the transform scene color.
float GetSceneColorHdrWeight(in TAAInputParameters InputParams, float4 SceneColor)
{
#if AA_YCOCG
	return HdrWeightY(SceneColor.x, InputParams.FrameExposureScale);
#endif
	return HdrWeight4(SceneColor.rgb, InputParams.FrameExposureScale);
}

#if AA_SAMPLE_CACHE_METHOD_LDS

// Total number of thread per group.
#define THREADGROUP_TOTAL (THREADGROUP_SIZEX * THREADGROUP_SIZEY)
#define LDS_BASE_TILE_WIDTH THREADGROUP_SIZEX


// Configuration of what should be prefetched.
// 1: use Load; 2: use gather4. 
#define AA_PRECACHE_SCENE_DEPTH 2

#define AA_PRECACHE_SCENE_COLOR 1


/////////////////////////// Depth tile constants ///////////////////////////////////////

// Number of texels arround the group tile for depth.
#define LDS_DEPTH_TILE_BORDER_SIZE (AA_CROSS)

// Width in texels of the depth tile cached into LDS.
#define LDS_DEPTH_TILE_WIDTH (LDS_BASE_TILE_WIDTH + 2 * LDS_DEPTH_TILE_BORDER_SIZE)

// Total number of texels cached in the depth tile.
#define LDS_DEPTH_ARRAY_SIZE (LDS_DEPTH_TILE_WIDTH * LDS_DEPTH_TILE_WIDTH)


////////////////////////// Scene color tile constants /////////////////////////////////////

// Number of texels arround the group tile for scene color.
#define LDS_COLOR_TILE_BORDER_SIZE (1)

// Width in texels of the depth tile cached into LDS.
#define LDS_COLOR_TILE_WIDTH (LDS_BASE_TILE_WIDTH + 2 * LDS_COLOR_TILE_BORDER_SIZE)

// Total number of texels cached in the scene color tile.
#define LDS_COLOR_ARRAY_SIZE (LDS_COLOR_TILE_WIDTH * LDS_COLOR_TILE_WIDTH)

// Precache GetSceneColorHdrWeight() into scene color's alpha channel.
#define AA_PRECACHE_SCENE_HDR_WEIGHT (AA_TONE)

// Number of scene color component that gets cached.
#define LDS_COLOR_COMPONENT_COUNT 4

/////////////////////////// Group shared global /////////////////////////////////////

// Size of the LDS to be allocated.
#define LDS_ARRAY_SIZE (LDS_COLOR_ARRAY_SIZE * LDS_COLOR_COMPONENT_COUNT)

// Some compilers may have issues optimising LDS store instructions, therefore we give the compiler a hint by using a float4 LDS.
#if defined(AA_PRECACHE_SCENE_DEPTH)
#define LDS_USE_FLOAT4_ARRAY 0
#else
#define LDS_USE_FLOAT4_ARRAY (LDS_COLOR_COMPONENT_COUNT == 4)
#endif

#if LDS_USE_FLOAT4_ARRAY
groupshared float4 GroupSharedArrayFloat4[LDS_ARRAY_SIZE / 4];
#else
groupshared float GroupSharedArray[LDS_ARRAY_SIZE];
#endif


/////////////////////////// Generic LDS tile functions /////////////////////////////////////

// Get the texel offset of a LDS tile's top left corner.
uint2 GetGroupTileTexelOffset(in TAAInputParameters InputParams, uint TileBorderSize)
{
    return InputParams.GroupId * uint2(THREADGROUP_SIZEX, THREADGROUP_SIZEY) - TileBorderSize;
}

// Get the index within the LDS array.
uint GetTileArrayIndexFromPixelOffset(in TAAInputParameters InputParams, int2 PixelOffset, uint TileBorderSize)
{
    uint2 TilePos = InputParams.GroupThreadId + uint2(PixelOffset + TileBorderSize);
    return TilePos.x + TilePos.y * (TileBorderSize * 2 + LDS_BASE_TILE_WIDTH);
}


///////////////////////////////// Share depth texture fetches////////////////////////////////

// Precache input scene depth into LDS.
void PrecacheInputSceneDepthToLDS(in TAAInputParameters InputParams)
{
	uint2 GroupTexelOffset = GetGroupTileTexelOffset(InputParams, LDS_DEPTH_TILE_BORDER_SIZE);
	
	#if AA_PRECACHE_SCENE_DEPTH == 1
	// Prefetch depth buffer using Load.
	{
		const uint LoadCount = (LDS_DEPTH_ARRAY_SIZE + THREADGROUP_TOTAL - 1) / THREADGROUP_TOTAL;
		
		uint LinearGroupThreadId = InputParams.GroupThreadIndex;

		[unroll]
		for (uint i = 0; i < LoadCount; i++)
		{
			uint2 TexelLocation =  GroupTexelOffset + uint2(
				LinearGroupThreadId % LDS_DEPTH_TILE_WIDTH,
				LinearGroupThreadId / LDS_DEPTH_TILE_WIDTH);

			if ((LinearGroupThreadId < LDS_DEPTH_ARRAY_SIZE) ||
				(i != LoadCount - 1) || (LDS_DEPTH_ARRAY_SIZE % THREADGROUP_TOTAL) == 0)
			{
				GroupSharedArray[LinearGroupThreadId] = SceneDepthTexture.Load(uint3(TexelLocation, 0)).x;
			}
			LinearGroupThreadId += THREADGROUP_TOTAL;
		}

	}

	#elif AA_PRECACHE_SCENE_DEPTH == 2
	// Prefetch depth buffer using Gather.
	{
		const uint LoadCount = (LDS_DEPTH_ARRAY_SIZE / 4 + THREADGROUP_TOTAL - 1) / THREADGROUP_TOTAL;
		
		uint LinearGroupThreadId = InputParams.GroupThreadIndex;
	
		[unroll]
		for (uint i = 0; i < LoadCount; i++)
		{
			uint2 TileDest = uint2(
				(2 * LinearGroupThreadId) % LDS_DEPTH_TILE_WIDTH,
				2 * ((2 * LinearGroupThreadId) / LDS_DEPTH_TILE_WIDTH));

			uint2 TexelLocation =  GroupTexelOffset + TileDest;

			uint DestI = TileDest.x + TileDest.y * LDS_DEPTH_TILE_WIDTH;

			if ((DestI < LDS_DEPTH_ARRAY_SIZE) || (i != LoadCount - 1) ||
				((LDS_DEPTH_ARRAY_SIZE / 4) % THREADGROUP_TOTAL) == 0)
			{
				float2 UV = float2(TexelLocation + 0.5) * InputViewSize.zw;
                float4 Depth = DepthTexture.Gather(BiLinearClampSampler, UV);
				GroupSharedArray[DestI + 1 * LDS_DEPTH_TILE_WIDTH + 0] = Depth.x;
				GroupSharedArray[DestI + 1 * LDS_DEPTH_TILE_WIDTH + 1] = Depth.y;
				GroupSharedArray[DestI + 0 * LDS_DEPTH_TILE_WIDTH + 1] = Depth.z;
				GroupSharedArray[DestI + 0 * LDS_DEPTH_TILE_WIDTH + 0] = Depth.w;
			}
			LinearGroupThreadId += THREADGROUP_TOTAL;
		}
	}
	#endif
}

float SampleCachedSceneDepthTexture(in TAAInputParameters InputParams, int2 PixelOffset)
{
#if AA_PRECACHE_SCENE_DEPTH
	return GroupSharedArray[GetTileArrayIndexFromPixelOffset(InputParams, PixelOffset, LDS_DEPTH_TILE_BORDER_SIZE)];
#else
    return SceneDepthTexture.SampleLevel(BiLinearClampSampler, InputParams.NearestBufferUV, 0, PixelOffset).r;
#endif
	
}



///////////////////////////////// Share color texture fetches ///////////////////////

#if defined(AA_PRECACHE_SCENE_COLOR)

// Return the index GroupSharedArray from a given ArrayIndex and ComponentId.
uint GetSceneColorLDSIndex(uint ArrayIndex, uint ComponentId)
{
    return ArrayIndex * LDS_COLOR_COMPONENT_COUNT + ComponentId;
}

// Precache input scene color into LDS.
void PrecacheInputSceneColorToLDS(in TAAInputParameters InputParams)
{
    const uint LoadCount = (LDS_COLOR_ARRAY_SIZE + THREADGROUP_TOTAL - 1) / THREADGROUP_TOTAL;
	
    uint LinearGroupThreadId = InputParams.GroupThreadIndex;
    uint2 GroupTexelOffset = GetGroupTileTexelOffset(InputParams, LDS_COLOR_TILE_BORDER_SIZE);
	
	[unroll]
    for (uint i = 0; i < LoadCount; i++)
    {
        uint2 TexelLocation = GroupTexelOffset + uint2(
			LinearGroupThreadId % LDS_COLOR_TILE_WIDTH,
			LinearGroupThreadId / LDS_COLOR_TILE_WIDTH);

        if ((LinearGroupThreadId < LDS_COLOR_ARRAY_SIZE) || (i != LoadCount - 1) ||
			(LDS_COLOR_ARRAY_SIZE % THREADGROUP_TOTAL) == 0)
        {

            int2 Coord = int2(TexelLocation);
            float4 RawColor = InputSceneColor.Load(uint3(Coord, 0));

            float4 Color = TransformCurrentFrameSceneColor(RawColor);

			// Precache scene color's HDR weight into alpha channel to reduce rcp() instructions in innerloops.
#if AA_PRECACHE_SCENE_HDR_WEIGHT
            Color.a = GetSceneColorHdrWeight(InputParams, Color);
#endif

#if LDS_USE_FLOAT4_ARRAY
			GroupSharedArrayFloat4[uint(LinearGroupThreadId)] = Color;

#else
            GroupSharedArray[GetSceneColorLDSIndex(uint(LinearGroupThreadId), 0)] = Color.r;
            GroupSharedArray[GetSceneColorLDSIndex(uint(LinearGroupThreadId), 1)] = Color.g;
            GroupSharedArray[GetSceneColorLDSIndex(uint(LinearGroupThreadId), 2)] = Color.b;
			
#if LDS_COLOR_COMPONENT_COUNT == 4
            GroupSharedArray[GetSceneColorLDSIndex(uint(LinearGroupThreadId), 3)] = Color.a;
#endif
			
#endif
        }
        LinearGroupThreadId += THREADGROUP_TOTAL;
    }
}

TAASceneColorSample SampleCachedSceneColorTexture(in TAAInputParameters InputParams, int2 PixelOffset)
{
    uint ArrayPos = GetTileArrayIndexFromPixelOffset(InputParams, PixelOffset, LDS_COLOR_TILE_BORDER_SIZE);
	
    TAASceneColorSample Sample;
	
#if LDS_USE_FLOAT4_ARRAY
	Sample.Color = GroupSharedArrayFloat4[ArrayPos];

#if AA_PRECACHE_SCENE_HDR_WEIGHT
	Sample.HdrWeight = Sample.Color.a;
	Sample.Color.a = 0;
#endif	
	
#else //!LDS_USE_FLOAT4_ARRAY
    Sample.Color.r = GroupSharedArray[GetSceneColorLDSIndex(ArrayPos, 0)];
    Sample.Color.g = GroupSharedArray[GetSceneColorLDSIndex(ArrayPos, 1)];
    Sample.Color.b = GroupSharedArray[GetSceneColorLDSIndex(ArrayPos, 2)];
    Sample.Color.a = 0;
	
#if AA_PRECACHE_SCENE_HDR_WEIGHT
    Sample.HdrWeight = GroupSharedArray[GetSceneColorLDSIndex(ArrayPos, 3)];
#endif

#endif
	
// if scene color weight was not precached in LDS, compute it.
#if !AA_PRECACHE_SCENE_HDR_WEIGHT
	Sample.HdrWeight = GetSceneColorHdrWeight(InputParams, Sample.Color);
#endif
	
	// Color has already been transformed in PrecacheInputSceneColor.
    return Sample;
}

#endif // AA_PRECACHE_SCENE_COLOR


void PrecacheInputSceneDepth(in TAAInputParameters InputParams)
{
#if defined(AA_PRECACHE_SCENE_DEPTH)
    PrecacheInputSceneDepthToLDS(InputParams);
    GroupMemoryBarrierWithGroupSync();
#endif
}

void PrecacheInputSceneColor(in TAAInputParameters InputParams)
{
#if defined(AA_PRECACHE_SCENE_DEPTH) && defined(AA_PRECACHE_SCENE_COLOR)
    GroupMemoryBarrierWithGroupSync();
#endif
#if defined(AA_PRECACHE_SCENE_COLOR)
    PrecacheInputSceneColorToLDS(InputParams);
    GroupMemoryBarrierWithGroupSync();
#endif
}

#endif //AA_SAMPLE_CACHE_METHOD_LDS


///////////////////////////////// TAA MAJOR FUNCTIONS ///////////////////////////////////////

// Filter input pixels.
void FilterCurrentFrameInputSamples(
	in TAAInputParameters InputParams,
	inout TAAIntermediaryResult IntermediaryResult)
{
#if 1
	{
        IntermediaryResult.Filtered = SampleCachedSceneColorTexture(InputParams, int2(0, 0)).Color;
        return;
    }
#endif

    float4 Filtered;

#if AA_SAMPLES == 9
			const uint SampleIndexes[9] = SquareIndexes3x3;
#else// AA_SAMPLES == 6
    const uint SampleIndexes[5] = PlusIndexes3x3;
#endif

    float NeighborsFinalWeight = 0;
    float4 NeighborsColor = 0;

        [unroll]
    for (uint i = 0; i < AA_SAMPLES; i++)
    {
			// Get the sample offset from the nearest input pixel.
        int2 SampleOffset;
		
			#if AA_UPSAMPLE && AA_SAMPLES == 6
				if (i == 5)
				{
					SampleOffset= sign(dKO);
				}
				else
			#endif
			{
            const uint SampleIndex = SampleIndexes[i];
            SampleOffset = Offsets3x3[SampleIndex];
        }
        float2 fSampleOffset = float2(SampleOffset);

			// Finds out the spatial weight of this input sample.
#if AA_SAMPLES == 9
			float SampleSpatialWeight = SampleWeights[i];

#elif AA_SAMPLES == 5
        float SampleSpatialWeight = PlusWeights[i];

#else
			#error Do not know how to compute filtering sample weight.

#endif

			// Fetch sample.
        TAASceneColorSample Sample = SampleCachedSceneColorTexture(InputParams, SampleOffset);
				
			// Finds out the sample's HDR weight.
#if AA_TONE
        float SampleHdrWeight = Sample.HdrWeight;
#else
			float SampleHdrWeight = 1;
#endif

        float SampleFinalWeight = SampleSpatialWeight * SampleHdrWeight;

			// Apply pixel.
        NeighborsColor += SampleFinalWeight * Sample.Color;
        NeighborsFinalWeight += SampleFinalWeight;
    }
		
#if AA_TONE
			// Reweight because SampleFinalWeight does not that have total sum = 1.
    Filtered = NeighborsColor * rcp(NeighborsFinalWeight);
#else
            Filtered = NeighborsColor;
#endif

    IntermediaryResult.Filtered = Filtered;
}


// Compute the neighborhood bounding box used to reject history.
void ComputeNeighborhoodBoundingbox(
	in TAAInputParameters InputParams,in TAAIntermediaryResult IntermediaryResult,
	out float4 OutNeighborMin,out float4 OutNeighborMax)
{
    float4 Neighbors[NeighborsCount];
	
    [unroll]
    for (uint i = 0; i < NeighborsCount; i++)
    {
        Neighbors[i] = SampleCachedSceneColorTexture(InputParams, Offsets3x3[i]).Color;
    }

    float4 NeighborMin;
    float4 NeighborMax;

#if AA_HISTORY_CLAMPING_BOX == HISTORY_CLAMPING_BOX_VARIANCE
	{
#if AA_SAMPLES == 9
			const uint SampleIndexes[9] = SquareIndexes3x3;
#else //AA_SAMPLES == 5
			const uint SampleIndexes[5] = PlusIndexes3x3;
#endif

		float4 m1 = 0;
		float4 m2 = 0;
		for( uint i = 0; i < AA_SAMPLES; i++ )
		{
			float4 SampleColor = Neighbors[ SampleIndexes[i] ];

			m1 += SampleColor;
			m2 += pow(SampleColor,2);
		}

		m1 *= (1.0 / AA_SAMPLES);
		m2 *= (1.0 / AA_SAMPLES);

		float4 StdDev = sqrt( abs(m2 - m1 * m1) );
		NeighborMin = m1 - 1.25 * StdDev;
		NeighborMax = m1 + 1.25 * StdDev;

		NeighborMin = min( NeighborMin, IntermediaryResult.Filtered );
		NeighborMax = max( NeighborMax, IntermediaryResult.Filtered );
	}
#elif AA_HISTORY_CLAMPING_BOX == HISTORY_CLAMPING_BOX_SAMPLE_DISTANCE
	// Do color clamping only within a radius.
	{
		float2 PPCo = InputParams.ViewportUV * InputViewSize.xy /*+ TemporalJitterPixels*/;
		float2 PPCk = floor(PPCo) + 0.5;
		float2 dKO = PPCo - PPCk;
		
		// Sample 4 is is always going to be considered anyway.
		NeighborMin = Neighbors[4];
		NeighborMax = Neighbors[4];
		
		// Reduce distance threshold as upsacale factor increase to reduce ghosting.
		float DistThreshold = 1.3;

#if AA_SAMPLES == 9
			const uint Indexes[9] = SquareIndexes3x3;
#else
			const uint Indexes[5] = PlusIndexes3x3;
#endif

		[unroll]
		for( uint i = 0; i < AA_SAMPLES; i++ )
		{
			uint NeightborId = Indexes[i];
			if (NeightborId != 4)
			{
				float2 dPP = float2(Offsets3x3[NeightborId]) - dKO;

				[flatten]
				if (dot(dPP, dPP) < (DistThreshold * DistThreshold))
				{
					NeighborMin = MinPayload(NeighborMin, Neighbors[NeightborId]);
					NeighborMax = MaxPayload(NeighborMax, Neighbors[NeightborId]);
				}
			}
		}
	}
#elif AA_HISTORY_CLAMPING_BOX == HISTORY_CLAMPING_BOX_MIN_MAX
	{
		NeighborMin = MinPayload3( Neighbors[1], Neighbors[3], Neighbors[4] );
		NeighborMin = MinPayload3( NeighborMin,  Neighbors[5], Neighbors[7] );

		NeighborMax = MaxPayload3( Neighbors[1], Neighbors[3], Neighbors[4] );
		NeighborMax = MaxPayload3( NeighborMax,  Neighbors[5], Neighbors[7] );
		
#if AA_SAMPLES == 6
		{
			float2 PPCo = InputParams.ViewportUV * InputViewSize.xy + TemporalJitterPixels;
			float2 PPCk = floor(PPCo) + 0.5;
			float2 dKO = PPCo - PPCk;
			
			int2 FifthNeighborOffset = SignFastInt(dKO);

			float4 FifthNeighbor;
			FifthNeighbor.Color = SampleCachedSceneColorTexture(InputParams, FifthNeighborOffset).Color;
			
			NeighborMin = MinPayload(NeighborMin, FifthNeighbor);
			NeighborMax = MaxPayload(NeighborMax, FifthNeighbor);
		}
#elif AA_SAMPLES == 9
		{
			float4 NeighborMinPlus = NeighborMin;
			float4 NeighborMaxPlus = NeighborMax;

			NeighborMin = MinPayload3( NeighborMin, Neighbors[0], Neighbors[2] );
			NeighborMin = MinPayload3( NeighborMin, Neighbors[6], Neighbors[8] );

			NeighborMax = MaxPayload3( NeighborMax, Neighbors[0], Neighbors[2] );
			NeighborMax = MaxPayload3( NeighborMax, Neighbors[6], Neighbors[8] );
		}
#endif
	}
#else
		#error Unknown history clamping box.
#endif

    OutNeighborMin = NeighborMin;
    OutNeighborMax = NeighborMax;
}

// Sample history.
float4 SampleHistory(in float2 HistoryScreenPosition)
{
    float4 RawHistory = 0;

	// Sample the history using Catmull-Rom to reduce blur on motion.
#if AA_BICUBIC
	{
		//[-1,1] to [0,1]
        float2 HistoryBufferUV = HistoryScreenPosition;

        CatmullRomSamples Samples = GetBicubic2DCatmullRomSamples(HistoryBufferUV, InputViewSize.xy, InputViewSize.zw);

		[unroll]
        for (uint i = 0; i < Samples.Count; i++)
        {
            float2 SampleUV = Samples.UV[i];

            RawHistory += HistoryColor.SampleLevel(BiLinearClampSampler, SampleUV, 0) * Samples.Weight[i];
        }
        RawHistory *= Samples.FinalMultiplier;
    }
	// Sample the history using bilinear sampler.
#else
	{
		//[-1,1] to [0,1]
		float2 HistoryBufferUV = HistoryScreenPosition;
		RawHistory = HistoryColor.SampleLevel(BiLinearClampSampler, HistoryBufferUV, 0);
	}
#endif

    float4 HistoryPayload;
    HistoryPayload = RawHistory;

    //HistoryPayload.Color.rgb *= HistoryPreExposureCorrection;

    HistoryPayload = TransformSceneColor(HistoryPayload);

    return HistoryPayload;
}

// Clamp history.
float4 ClampHistory(float4 History, float4 NeighborMin, float4 NeighborMax)
{
#if !AA_CLAMP
		return History;
#else	
    History = clamp(History, NeighborMin, NeighborMax);
    return History;
#endif
}


float2 STtoUV(float2 ST)
{
    return (ST + 0.5f) * OutputViewportSize.zw;
}

float MaxOf(float4 Depths)
{
    return max(max(Depths.x, Depths.y), max(Depths.z, Depths.w));
}

float3 ClipColor(float3 Color, float3 BoxMin, float3 BoxMax, float Dilation = 1.0)
{
    float3 BoxCenter = (BoxMax + BoxMin) * 0.5;
    float3 HalfDim = (BoxMax - BoxMin) * 0.5 * Dilation + 0.001;
    float3 Displacement = Color - BoxCenter;
    float3 Units = abs(Displacement / HalfDim);
    float MaxUnit = max(max(Units.x, Units.y), max(Units.z, 1.0));
    return BoxCenter + Displacement / MaxUnit;
}

void ApplyTemporalBlend(uint2 ST, float4 CurrentColor, float2 DepthOffset,float3 Min, float3 Max)
{
    float CompareDepth = 0;

    // Get the velocity of the closest pixel in the '+' formation
    float3 Velocity = UnpackVelocity(VelocityBuffer[ST + DepthOffset]);

    CompareDepth += Velocity.z;

    // The temporal depth is the actual depth of the pixel found at the same reprojected location.
    float TemporalDepth = MaxOf(PreDepthTexture.Gather(BiLinearClampSampler, STtoUV(ST + Velocity.xy + ViewportJitter))) + 1e-3;

    // Fast-moving pixels cause motion blur and probably don't need TAA
    float RcpSpeedLimiter = 1 / 64.0f;
    float SpeedFactor = saturate(1.0 - length(Velocity.xy) * RcpSpeedLimiter);

    // Fetch temporal color.  Its "confidence" weight is stored in alpha.
    float4 Temp = SampleHistory(STtoUV(ST + Velocity.xy));
    float3 TemporalColor = Temp.rgb;
    float TemporalWeight = Temp.w;

    // Pixel colors are pre-multiplied by their weight to enable bilinear filtering.  Divide by weight to recover color.
    TemporalColor /= max(TemporalWeight, 1e-6);

    // Clip the temporal color to the current neighborhood's bounding box.  Increase the size of the bounding box for
    // stationary pixels to avoid rejecting noisy specular highlights.
    TemporalColor = ClipColor(TemporalColor, Min, Max, lerp(1.0, 4.0, SpeedFactor * SpeedFactor));

    // Update the confidence term based on speed and disocclusion
    TemporalWeight *= SpeedFactor* step(CompareDepth, TemporalDepth);

    // Blend previous color with new color based on confidence.  Confidence steadily grows with each iteration
    // until it is broken by movement such as through disocclusion, color changes, or moving beyond the resolution
    // of the velocity buffer.
    //TemporalColor = ITM(lerp(TM(YCoCgToRGB(CurrentColor.xyz)), TM(YCoCgToRGB(TemporalColor)), TemporalWeight));
    TemporalColor = lerp(YCoCgToRGB(CurrentColor.xyz), YCoCgToRGB(TemporalColor), TemporalWeight);
	
    // Update weight
    TemporalWeight = saturate(rcp(2.0 - TemporalWeight));

    // Quantize weight to what is representable
    TemporalWeight = f16tof32(f32tof16(TemporalWeight));

    OutTexture[ST] = float4(TemporalColor, 1) * TemporalWeight;
}


///////////////////////////////// TAA MAIN FUNCTION //////////////////////////

void TemporalAASample(uint2 DispatchThreadId,uint2 GroupId, uint2 GroupThreadId, uint GroupThreadIndex, float2 ViewportUV, float FrameExposureScale)
{
    TAAInputParameters InputParams;

	// Per frame setup.
	{
        InputParams.FrameExposureScale = FrameExposureScale;

        InputParams.GroupId = GroupId;
        InputParams.GroupThreadId = GroupThreadId;
        InputParams.GroupThreadIndex = GroupThreadIndex;
        InputParams.ViewportUV = ViewportUV;
        InputParams.ScreenPos = ViewportUVToScreenPos(ViewportUV);
        InputParams.NearestBufferUV = ViewportUV;
    }
	
	
	// Setup intermediary results.
    TAAIntermediaryResult IntermediaryResult = CreateIntermediaryResult();
    IntermediaryResult.Filtered = float4(1, 1, 1, 1);
	// FIND MOTION OF PIXEL AND NEAREST IN NEIGHBORHOOD
	
    float3 PosN; // Position of this pixel, possibly later nearest pixel in neighborhood.
    PosN.xy = InputParams.ScreenPos;

	//Pre cache SceneDepth
    PrecacheInputSceneDepth(InputParams);

    PosN.z = SampleCachedSceneDepthTexture(InputParams, int2(0, 0));
	
	// Screen position of minimum depth.
    float2 VelocityOffset = float2(0.0, 0.0);
    float2 DepthOffset = float2(AA_CROSS, AA_CROSS);
    float  DepthOffsetXx = float(AA_CROSS);
	#if AA_CROSS 
	{
		// For motion vector, use camera/dynamic motion from min depth pixel in pattern around pixel.
		// This enables better quality outline on foreground against different motion background.
		// Larger 2 pixel distance "x" works best (because AA dilates surface).
        float4 Depths;
        Depths.x = SampleCachedSceneDepthTexture(InputParams, int2(-AA_CROSS, -AA_CROSS));
        Depths.y = SampleCachedSceneDepthTexture(InputParams, int2(AA_CROSS, -AA_CROSS));
        Depths.z = SampleCachedSceneDepthTexture(InputParams, int2(-AA_CROSS, AA_CROSS));
        Depths.w = SampleCachedSceneDepthTexture(InputParams, int2(AA_CROSS, AA_CROSS));

			// Nearest depth is the largest depth (depth surface 0=far, 1=near).
        if (Depths.x < Depths.y)
        {
            DepthOffsetXx = -AA_CROSS;
        }
        if (Depths.z < Depths.w)
        {
            DepthOffset.x = -AA_CROSS;
        }
        float DepthsXY = min(Depths.x, Depths.y);
        float DepthsZW = min(Depths.z, Depths.w);
        if (DepthsXY < DepthsZW)
        {
            DepthOffset.y = -AA_CROSS;
            DepthOffset.x = DepthOffsetXx;
        }
        float DepthsXYZW = min(DepthsXY, DepthsZW);
        if (DepthsXYZW < PosN.z)
        {
				// This is offset for reading from velocity texture.
				// This supports half or fractional resolution velocity textures.
				// With the assumption that UV position scales between velocity and color.
            VelocityOffset = DepthOffset * InputViewSize.zw;
            PosN.z = DepthsXYZW;
        }

    }
	#endif	// AA_CROSS
	
	
	// Camera motion for pixel or nearest pixel (in ScreenPos space).
    bool OffScreen = false;
    float Velocity = 0;
    float HistoryBlur = 0;
    float2 HistoryScreenPosition = InputParams.ScreenPos;


    float4 ThisClip = float4(PosN.xy, PosN.z, 1);
    float4 PrevClip = mul(CurrentToPrev, ThisClip);
    float2 PrevScreen = PrevClip.xy / PrevClip.w;
    float2 BackN = PosN.xy - PrevScreen;

    float2 BackTemp = BackN * OutputViewportSize.xy;

#if AA_DYNAMIC
	{
        int2 sampleLocation = floor(InputParams.NearestBufferUV + VelocityOffset);
        BackN = UnpackVelocity(VelocityBuffer[sampleLocation]).xy;
        BackTemp = BackN * OutputViewportSize.xy;
    }
#endif

    Velocity = sqrt(dot(BackTemp, BackTemp));

	// Easier to do off screen check before conversion.
	// This converts back projection vector to [-1 to 1] offset in viewport.
    HistoryScreenPosition = InputParams.ScreenPos - BackN / (Velocity == 0 ? 1 : Velocity);

	
    //return float4(HistoryScreenPosition, 0, 0);
	// Detect if HistoryBufferUV would be outside of the viewport.
    OffScreen = max(abs(HistoryScreenPosition.x), abs(HistoryScreenPosition.y)) >= 1.0;
	
	// Precache input scene color.
    PrecacheInputSceneColor(InputParams);
	
	// Filter input.
	FilterCurrentFrameInputSamples(InputParams,IntermediaryResult);
	
	// Compute neighborhood bounding box.
    float4 NeighborMin;
    float4 NeighborMax;

    ComputeNeighborhoodBoundingbox(InputParams, IntermediaryResult, NeighborMin, NeighborMax);
	
	// Whether the feedback needs to be reset.
    bool IgnoreHistory = OffScreen;

	// COMPUTE BLEND AMOUNT
	
    ApplyTemporalBlend(DispatchThreadId, IntermediaryResult.Filtered, DepthOffset, NeighborMin.rgb, NeighborMax.rgb);
}


[numthreads(THREADGROUP_SIZEX, THREADGROUP_SIZEY, 1)]
void main(uint2 DispatchThreadId : SV_DispatchThreadID,
	uint2 GroupId : SV_GroupID,
	uint2 GroupThreadId : SV_GroupThreadID,
	uint GroupThreadIndex : SV_GroupIndex)
{
    float2 ViewportUV = (float2(DispatchThreadId) + 0.5f) * OutputViewportSize.zw;
    float FrameExposureScale = 1.0f;
    TemporalAASample(DispatchThreadId, GroupId, GroupThreadId, GroupThreadIndex, ViewportUV, FrameExposureScale);
}