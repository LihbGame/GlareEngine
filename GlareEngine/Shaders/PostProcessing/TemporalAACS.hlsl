#include "../Misc/CommonResource.hlsli"

cbuffer TAAConstantBuffer :			 register(b1)
{
	float4 OutputViewportSize;
	float4 OutputViewportRect;

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

// Disable any in code cache.
#define AA_SAMPLE_CACHE_METHOD_DISABLE 0

// Caches 3x3 Neighborhood into VGPR (although my have corner optimised away).
#define AA_SAMPLE_CACHE_METHOD_VGPR_3X3 1

// Prefetches scene color into 10x10 LDS tile.
#define AA_SAMPLE_CACHE_METHOD_LDS 2


//// Payload of the history. History might still have addtional TAA internals.

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
#define AA_UPSAMPLE 0
//Change the upsampling filter size when history is rejected that reduce blocky output pixels
#define AA_UPSAMPLE_ADAPTIVE_FILTERING 1

//Cross distance in pixels used in depth search X pattern.
#if AA_UPSAMPLE
#define AA_CROSS 1
#else
#define AA_CROSS 2
#endif

//CACHE METHOD
#define AA_SAMPLE_CACHE_METHOD (AA_SAMPLE_CACHE_METHOD_LDS)

#if TAA_QUALITY == TAA_QUALITY_LOW
//Always enable scene color filtering
#define AA_FILTERED 0
//Num samples of current frame
#define AA_SAMPLES 6

#elif TAA_QUALITY == TAA_QUALITY_MEDIUM
#define AA_FILTERED 1
#define AA_SAMPLES 6

#elif TAA_QUALITY == TAA_QUALITY_HIGH
#define AA_HISTORY_CLAMPING_BOX (HISTORY_CLAMPING_BOX_SAMPLE_DISTANCE)
#define AA_FILTERED 1
//Antighosting using dynamic mask
#define AA_DYNAMIC_ANTIGHOST 1
#define AA_SAMPLES 9

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

	// Radius of the circle of confusion for DOF.
	float CocRadius;
};

TAAHistoryPayload MulPayload(in TAAHistoryPayload Payload, in float x)
{
	Payload.Color *= x;
	Payload.CocRadius *= x;
	return Payload;
}

TAAHistoryPayload AddPayload(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1)
{
	Payload0.Color += Payload1.Color;
	Payload0.CocRadius += Payload1.CocRadius;
	return Payload0;
}

TAAHistoryPayload MinPayload(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1)
{
	Payload0.Color = min(Payload0.Color, Payload1.Color);
	Payload0.CocRadius = min(Payload0.CocRadius, Payload1.CocRadius);
	return Payload0;
}

TAAHistoryPayload MaxPayload(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1)
{
	Payload0.Color = max(Payload0.Color, Payload1.Color);
	Payload0.CocRadius = max(Payload0.CocRadius, Payload1.CocRadius);
	return Payload0;
}

TAAHistoryPayload MinPayload3(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1, in TAAHistoryPayload Payload2)
{
	Payload0.Color = min(min(Payload0.Color, Payload1.Color), Payload2.Color);
	Payload0.CocRadius = min(min(Payload0.CocRadius, Payload1.CocRadius), Payload2.CocRadius);
	return Payload0;
}

TAAHistoryPayload MaxPayload3(in TAAHistoryPayload Payload0, in TAAHistoryPayload Payload1, in TAAHistoryPayload Payload2)
{
	Payload0.Color = max(max(Payload0.Color, Payload1.Color), Payload2.Color);
	Payload0.CocRadius = max(max(Payload0.CocRadius, Payload1.CocRadius), Payload2.CocRadius);
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

	// Cache of neightbors' transformed scene color.
#if AA_SAMPLE_CACHE_METHOD == AA_SAMPLE_CACHE_METHOD_VGPR_3X3
	float4				CachedNeighbors[NeighborsCount];
#endif
};



// Transformed scene color's data for a sample.
struct FTAASceneColorSample
{
	// Transformed scene color and alpha channel.
	float4 Color;

	// Radius of the circle of confusion for DOF.
	float CocRadius;

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

//3x3 NEIGHTBORS CACHING
#if AA_SAMPLE_CACHE_METHOD == AA_SAMPLE_CACHE_METHOD_VGPR_3X3

#define AA_PRECACHE_SCENE_COLOR 1

void PrecacheInputSceneColor(inout TAAInputParameters InputParams)
{
	// Precache 3x3 input scene color into TAAInputParameters::CachedNeighbors.
	UNROLL
		for (uint i = 0; i < NeighborsCount; i++)
		{
			int2 Coord = int2(InputParams.NearestBufferUV * InputSceneColorSize.xy) + Offsets3x3[i];
			Coord = clamp(Coord, InputMinPixelCoord, InputMaxPixelCoord);
			InputParams.CachedNeighbors[i] = TransformCurrentFrameSceneColor(InputSceneColor[Coord]);
		}
}

TAASceneColorSample SampleCachedSceneColorTexture(
	inout TAAInputParameters InputParams,
	int2 PixelOffset)
{
	// PixelOffset is const at compile time. Therefore all this computaton is actually free.
	uint NeighborsId = uint(4 + PixelOffset.x + PixelOffset.y * 3);
	TAASceneColorSample Sample;

	Sample.Color = InputParams.CachedNeighbors[NeighborsId];
	Sample.CocRadius = 0;
	Sample.HdrWeight = GetSceneColorHdrWeight(InputParams, Sample.Color);
	return Sample;
}

// LDS CACHING
#elif AA_SAMPLE_CACHE_METHOD == AA_SAMPLE_CACHE_METHOD_LDS

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




#endif


[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}