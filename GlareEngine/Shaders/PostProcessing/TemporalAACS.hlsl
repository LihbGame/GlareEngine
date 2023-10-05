#include "../Misc/CommonResource.hlsli"


Texture2D SceneDepthTexture : register(t0);
Texture2D InputSceneColor	: register(t1);

SamplerState SceneDepthTextureSampler : register(s1);

cbuffer TAAConstantBuffer : register(b1)
{
    float4 OutputViewportSize;
    float4 OutputViewportRect;
	
    float4 InputSceneColorSize;
	
    float4 InputViewSize;
    float2 InputViewMin;
	// Temporal jitter at the pixel.
    float2 TemporalJitterPixels;
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
//Whether the history buffer UV should be manually clamped
#define AA_MANUALLY_CLAMP_HISTORY_UV 1
//Tone map to kill fireflies
#define AA_TONE 1
//Use YCoCg
#define AA_YCOCG 1

//Upsample the output
#define AA_UPSAMPLE 1

#define UPSCALE_FACTOR 2.0

//Change the upsampling filter size when history is rejected that reduce blocky output pixels
#define AA_UPSAMPLE_ADAPTIVE_FILTERING 1

//Cross distance in pixels used in depth search X pattern.
#if AA_UPSAMPLE
#define AA_CROSS 1
#else
#define AA_CROSS 2
#endif

#if TAA_QUALITY == TAA_QUALITY_LOW
//Always enable scene color filtering
#define AA_FILTERED 0
//Num samples of current frame
#if AA_UPSAMPLE
#define AA_SAMPLES 6
#endif

#elif TAA_QUALITY == TAA_QUALITY_MEDIUM
#define AA_FILTERED 1
#if AA_UPSAMPLE
#define AA_SAMPLES 6
#endif

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

#if AA_UPSAMPLE
static const int2 Offsets2x2[4] =
{
	int2(0,  0),
	int2(1,  0),
	int2(0,  1),
	int2(1,  1),
};

//Indexes of the 2x2 square.
static const uint SquareIndexes2x2[4] = { 0, 1, 2, 3 };

#endif // AA_UPSAMPLE


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



// Payload of the TAA's history.
struct TAAHistoryPayload
{
	// Transformed scene color and alpha channel.
	float4 Color;
};

TAAHistoryPayload MulPayload(in TAAHistoryPayload Payload, in float x)
{
	Payload.Color *= x;
	return Payload;
}

TAAHistoryPayload AddPayload(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1)
{
	Payload0.Color += Payload1.Color;
	return Payload0;
}

TAAHistoryPayload MinPayload(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1)
{
	Payload0.Color = min(Payload0.Color, Payload1.Color);
	return Payload0;
}

TAAHistoryPayload MaxPayload(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1)
{
	Payload0.Color = max(Payload0.Color, Payload1.Color);
	return Payload0;
}

TAAHistoryPayload MinPayload3(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1, in TAAHistoryPayload Payload2)
{
	Payload0.Color = min(min(Payload0.Color, Payload1.Color), Payload2.Color);
	return Payload0;
}

TAAHistoryPayload MaxPayload3(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1, in TAAHistoryPayload Payload2)
{
	Payload0.Color = max(max(Payload0.Color, Payload1.Color), Payload2.Color);
	return Payload0;
}

struct TAAIntermediaryResult
{
	// The filtered input.
	TAAHistoryPayload	Filtered;

	// Temporal weight of the filtered input.
	float				FilteredTemporalWeight;

	// 1 / filtering kernel scale factor for AA_UPSAMPLE_ADAPTIVE_FILTERING.
	float				InvFilterScaleFactor;
};


// Create intermediary result.
TAAIntermediaryResult CreateIntermediaryResult()
{
	TAAIntermediaryResult IntermediaryResult = (TAAIntermediaryResult)(0.0);
	IntermediaryResult.FilteredTemporalWeight = 1;
	IntermediaryResult.InvFilterScaleFactor = 1;
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

#if AA_UPSAMPLE
	// Buffer UV of the nearest top left input pixel.
	float2				NearestTopLeftBufferUV;
#endif

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
float GetSceneColorLuma4(float4 SceneColor)
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
#if !AA_UPSAMPLE
#define AA_PRECACHE_SCENE_DEPTH 2
#endif

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

#if AA_UPSAMPLE

// Get the pixel coordinate of the nearest input pixel for group's thread 0.
float2 GetGroupThread0InputPixelCoord(in TAAInputParameters InputParams)
{
	// Output pixel center position of the group thread index 0, relative to top left corner of the viewport.
	float2 Thread0SvPosition = InputParams.GroupId * uint2(THREADGROUP_SIZEX, THREADGROUP_SIZEY) + 0.5;

	// Output pixel's viewport UV group thread index 0.
	float2 Thread0ViewportUV = Thread0SvPosition * OutputViewportSize.zw;

	// Pixel coordinate of the center of output pixel O in the input viewport.
	float2 Thread0PPCo = Thread0ViewportUV * InputViewSize.xy + TemporalJitterPixels;

	// Pixel coordinate of the center of the nearest input pixel.
	float2 Thread0PPCk = floor(Thread0PPCo) + 0.5;

	return InputViewMin.xy + Thread0PPCk;
}

#endif

// Get the texel offset of a LDS tile's top left corner.
uint2 GetGroupTileTexelOffset(in TAAInputParameters InputParams, uint TileBorderSize)
{
#if AA_UPSAMPLE
	{
		// Pixel coordinate of the center of the nearest input pixel K.
		float2 Thread0PPCk = GetGroupThread0InputPixelCoord(InputParams);

		return uint2(floor(Thread0PPCk) - TileBorderSize);
	}
#else // !AA_UPSAMPLE
	{
		return OutputViewportRect.xy + InputParams.GroupId * uint2(THREADGROUP_SIZEX, THREADGROUP_SIZEY) - TileBorderSize;
	}
#endif
}

// Get the index within the LDS array.
uint GetTileArrayIndexFromPixelOffset(in TAAInputParameters InputParams, int2 PixelOffset, uint TileBorderSize)
{
#if AA_UPSAMPLE
	{
		const float2 RowMultiplier = float2(1, TileBorderSize * 2 + LDS_BASE_TILE_WIDTH);

		float2 Thread0PPCk = GetGroupThread0InputPixelCoord(InputParams);
		float2 PPCk = InputParams.NearestBufferUV * InputSceneColorSize.xy;

		float2 TilePos = floor(PPCk) - floor(Thread0PPCk);
		return uint(dot(TilePos, RowMultiplier) + dot(float2(PixelOffset)+float(TileBorderSize), RowMultiplier));
	}
#else
	{
		uint2 TilePos = InputParams.GroupThreadId + uint2(PixelOffset + TileBorderSize);
		return TilePos.x + TilePos.y * (TileBorderSize * 2 + LDS_BASE_TILE_WIDTH);
	}
#endif
}


///////////////////////////////// Share depth texture fetches////////////////////////////////

#if defined(AA_PRECACHE_SCENE_DEPTH)

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
				float2 UV = float2(TexelLocation + 0.5) * InputSceneColorSize.zw;
				float4 Depth = SceneDepthTexture.Gather(SceneDepthTextureSampler, UV);
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
	return GroupSharedArray[GetTileArrayIndexFromPixelOffset(InputParams, PixelOffset, LDS_DEPTH_TILE_BORDER_SIZE)];
}

#endif // define(AA_PRECACHE_SCENE_DEPTH)


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
	
#if AA_UPSAMPLE  //use float uv
    float LinearGroupThreadId = float(InputParams.GroupThreadIndex);
    float2 Thread0PPCk = GetGroupThread0InputPixelCoord(InputParams);
    float2 GroupTexelOffset = Thread0PPCk - LDS_COLOR_TILE_BORDER_SIZE;
#else
	uint LinearGroupThreadId = InputParams.GroupThreadIndex;
	uint2 GroupTexelOffset = GetGroupTileTexelOffset(InputParams, LDS_COLOR_TILE_BORDER_SIZE);
#endif
	
	[unroll]
    for (uint i = 0; i < LoadCount; i++)
    {
#if AA_UPSAMPLE //use float uv
        float Y = floor(LinearGroupThreadId * (1.0 / LDS_COLOR_TILE_WIDTH));
        float X = LinearGroupThreadId - LDS_COLOR_TILE_WIDTH * Y;

        float2 TexelLocation = GroupTexelOffset + float2(X, Y);
#else
		uint2 TexelLocation =  GroupTexelOffset + uint2(
			LinearGroupThreadId % LDS_COLOR_TILE_WIDTH,
			LinearGroupThreadId / LDS_COLOR_TILE_WIDTH);
#endif

        if ((LinearGroupThreadId < LDS_COLOR_ARRAY_SIZE) || (i != LoadCount - 1) ||
			(LDS_COLOR_ARRAY_SIZE % THREADGROUP_TOTAL) == 0)
        {
#if AA_UPSAMPLE //use float uv
            int2 Coord = TexelLocation;
            float4 RawColor = InputSceneColor[Coord];
#else
			int2 Coord = int2(TexelLocation);
			float4 RawColor = InputSceneColor.Load(uint3(Coord, 0));
#endif

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


/////////////////////////////////Temporal Upsampling ///////////////////////

#if AA_UPSAMPLE

// Returns the weight of a pixels at a coordinate PixelDelta from the PDF highest point.
float ComputeSampleWeigth(in TAAIntermediaryResult IntermediaryResult, float2 PixelDelta)
{
    float u2 = UPSCALE_FACTOR * UPSCALE_FACTOR;

	// The point of InvFilterScaleFactor is to blur current frame scene color when upscaling.
    u2 *= (IntermediaryResult.InvFilterScaleFactor * IntermediaryResult.InvFilterScaleFactor);

	// 1 - 1.9 * x^2 + 0.9 * x^4
    float x2 = saturate(u2 * dot(PixelDelta, PixelDelta));
    return (0.905 * x2 - 1.9) * x2 + 1;
}


// Returns the weight of a pixels at a coordinate PixelDelta from the PDF highest point.
float ComputePixelWeigth(in TAAIntermediaryResult IntermediaryResult, float2 PixelDelta)
{
    float u2 = UPSCALE_FACTOR * UPSCALE_FACTOR;

	// The point of InvFilterScaleFactor is to blur current frame scene color when upscaling.
    u2 *= (IntermediaryResult.InvFilterScaleFactor * IntermediaryResult.InvFilterScaleFactor);

	// 1 - 1.9 * x^2 + 0.9 * x^4
    float x2 = saturate(u2 * dot(PixelDelta, PixelDelta));
    float r = (0.905 * x2 - 1.9) * x2 + 1;

	// Multiply pixel weight ^ 2 by upscale factor because have only a probability = screen percentage ^ 2 to return 1.
    return u2 * r;
}

#endif // AA_UPSAMPLE


///////////////////////////////// TAA MAJOR FUNCTIONS ///////////////////////////////////////

// Filter input pixels.
void FilterCurrentFrameInputSamples(
	in TAAInputParameters InputParams,
	inout TAAIntermediaryResult IntermediaryResult)
{
#if !AA_FILTERED
	{
        IntermediaryResult.Filtered.Color = SampleCachedSceneColorTexture(InputParams, int2(0, 0)).Color;
        return;
    }
#endif

    TAAHistoryPayload Filtered;

	{
#if AA_UPSAMPLE
		// Pixel coordinate of the center of output pixel O in the input viewport.
        float2 PPCo = InputParams.ViewportUV * InputViewSize.xy + TemporalJitterPixels;

		// Pixel coordinate of the center of the nearest input pixel.
        float2 PPCk = floor(PPCo) + 0.5;
		
		// Vector in pixel between pixel Center and Jitter.
        float2 dKO = PPCo - PPCk;
#endif
		
#if AA_SAMPLES == 9
			const uint SampleIndexes[9] = SquareIndexes3x3;
#else// AA_SAMPLES == 6
			const uint SampleIndexes[5] = PlusIndexes3x3;
#endif

        float NeighborsHdrWeight = 0;
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
#if AA_UPSAMPLE
				// Compute the pixel delta between output pixels and input pixel I.
				//  Note: abs() is unecessary because of the dot(dPP, dPP) latter on.
            float2 dPP = fSampleOffset - dKO;

            float SampleSpatialWeight = ComputeSampleWeigth(IntermediaryResult, dPP);

#elif AA_SAMPLES == 9
				float SampleSpatialWeight = GET_SCALAR_ARRAY_ELEMENT(SampleWeights, i);

#elif AA_SAMPLES == 5
				float SampleSpatialWeight = GET_SCALAR_ARRAY_ELEMENT(PlusWeights, i);

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

            NeighborsHdrWeight += SampleSpatialWeight * SampleHdrWeight;
        }
		
#if AA_UPSAMPLE
			// Compute the temporal weight of the output pixel.
        IntermediaryResult.FilteredTemporalWeight = ComputePixelWeigth(IntermediaryResult, dKO);
#endif
    }

    IntermediaryResult.Filtered = Filtered;
}


// Compute the neighborhood bounding box used to reject history.
void ComputeNeighborhoodBoundingbox(
	in TAAInputParameters InputParams,in TAAIntermediaryResult IntermediaryResult,
	out TAAHistoryPayload OutNeighborMin,out TAAHistoryPayload OutNeighborMax)
{
    TAAHistoryPayload Neighbors[NeighborsCount];
	
    [unroll]
    for (uint i = 0; i < NeighborsCount; i++)
    {
        Neighbors[i].Color = SampleCachedSceneColorTexture(InputParams, Offsets3x3[i]).Color;
    }

    TAAHistoryPayload NeighborMin;
    TAAHistoryPayload NeighborMax;

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
			m2 += Pow2( SampleColor );
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
		float2 PPCo = InputParams.ViewportUV * InputViewSize.xy + TemporalJitterPixels;
		float2 PPCk = floor(PPCo) + 0.5;
		float2 dKO = PPCo - PPCk;
		
		// Sample 4 is is always going to be considered anyway.
		NeighborMin = Neighbors[4];
		NeighborMax = Neighbors[4];
		
		// Reduce distance threshold as upsacale factor increase to reduce ghosting.
#if AA_UPSAMPLE
		float DistthresholdLerp = UPSCALE_FACTOR - 1;
#else
		float DistthresholdLerp = 0;
#endif
		float DistThreshold = lerp(1.51, 1.3, DistthresholdLerp);

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

			TAAHistoryPayload FifthNeighbor;
			FifthNeighbor.Color = SampleCachedSceneColorTexture(InputParams, FifthNeighborOffset).Color;
			
			NeighborMin = MinPayload(NeighborMin, FifthNeighbor);
			NeighborMax = MaxPayload(NeighborMax, FifthNeighbor);
		}
#elif AA_SAMPLES == 9
		{
			TAAHistoryPayload NeighborMinPlus = NeighborMin;
			TAAHistoryPayload NeighborMaxPlus = NeighborMax;

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




[numthreads(THREADGROUP_SIZEX, THREADGROUP_SIZEY, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}