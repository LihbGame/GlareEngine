#include "../Misc/CommonResource.hlsli"

//Using a one-dimensional thread group, this blur requires horizontal and vertical scheduling twice, 
//but this uses less shared space, or using a two-dimensional thread group, 
//horizontal and vertical blurs can be computed in one schedule at the same time, 
//but this requires more shared memory, especially when the blur range is wider.
#define BLUR_GAUSSIAN_THREADCOUNT 256

#ifndef BLUR_FORMAT
#define BLUR_FORMAT float4
#endif

Texture2D<BLUR_FORMAT> Input		: register(t0);

Texture2D<float> LinearDepth		: register(t1);

RWTexture2D<BLUR_FORMAT> Output		: register(u0);

SamplerState BiLinearClampSampler	: register(s0);

cbuffer BlurConstant				: register(b1)
{
	float2		InverseDimensions;
	int2		Dimensions;
	int			IsHorizontalBlur;
}


// Calculate gaussian weights: http://dev.theomader.com/gaussian-kernel-calculator/
#ifdef BLUR_WIDE
//Radius =16
static const int GAUSS_KERNEL = 33;
static const float GaussianWeightsNormalized[GAUSS_KERNEL] = 
{
	0.004013,0.005554,0.007527,0.00999,0.012984,0.016524,0.020594,0.025133,0.030036,0.035151,0.040283,0.045207,0.049681,0.053463,0.056341,0.058141,
	0.058754,
	0.058141,0.056341,0.053463,0.049681,0.045207,0.040283,0.035151,0.030036,0.025133,0.020594,0.016524,0.012984,0.00999,0.007527,0.005554,0.004013
};

static const int GaussianOffsets[GAUSS_KERNEL] = 
{
	-16,-15,-14,-13,-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,
	0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};
#else
//Radius =4
static const int GAUSS_KERNEL = 9;
static const float GaussianWeightsNormalized[GAUSS_KERNEL] = 
{
	0.004112,0.026563,0.100519,0.223215,
	0.29118,
	0.223215,0.100519,0.026563,0.004112
};
static const int GaussianOffsets[GAUSS_KERNEL] = { -4,-3,-2,-1,0,1,2,3,4 };
#endif // BLUR_WIDE

static const int TILE_BORDER = GAUSS_KERNEL / 2;
static const int CACHE_SIZE = TILE_BORDER * 2 + BLUR_GAUSSIAN_THREADCOUNT;

groupshared BLUR_FORMAT ColorCache[CACHE_SIZE];

#ifdef BILATERAL
groupshared float DepthCache[CACHE_SIZE];
#endif // BILATERAL

[numthreads(BLUR_GAUSSIAN_THREADCOUNT, 1, 1)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex)
{
	uint2 TileStart		= Gid.xy;
	float2 Direction	= float2(0, 0);

	[flatten]
	if (IsHorizontalBlur)
	{
		TileStart.x *= BLUR_GAUSSIAN_THREADCOUNT;
		Direction = float2(1, 0);
	}
	else
	{
		TileStart.y *= BLUR_GAUSSIAN_THREADCOUNT;
		Direction = float2(0, 1);
	}

	int i;
	for (i = GI; i < CACHE_SIZE; i += BLUR_GAUSSIAN_THREADCOUNT)
	{
		const float2 uv		= (TileStart + 0.5f + Direction * (i - TILE_BORDER)) * InverseDimensions;
		ColorCache[i]		= Input.SampleLevel(BiLinearClampSampler, uv, 0);

#ifdef BILATERAL
		DepthCache[i]		= LinearDepth.SampleLevel(BiLinearClampSampler, uv, 0);
#endif // BILATERAL
	}

	GroupMemoryBarrierWithGroupSync();

	const uint2 PixelPos = TileStart + GI * Direction;
	if (PixelPos.x >= Dimensions.x || PixelPos.y >= Dimensions.y)
	{
		return;
	}


}