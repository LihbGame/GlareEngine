
//Qualities
#define TAA_QUALITY_LOW		0
#define TAA_QUALITY_MEDIUM	1
#define TAA_QUALITY_HIGH	2


////Clamping method for scene color

// Min max neighboorhing samples.
#define HISTORY_CLAMPING_BOX_MIN_MAX 0

// Variance computed from neighboorhing samples.
#define HISTORY_CLAMPING_BOX_VARIANCE 1

// Min max samples that are within distance from output pixel. 
#define HISTORY_CLAMPING_BOX_SAMPLE_DISTANCE 2


//Quality setting
#define TAA_QUALITY TAA_QUALITY_HIGH

//Bicubic filter history
#define AA_BICUBIC 1
//Cross distance in pixels used in depth search X pattern.
#define AA_CROSS 2
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
//Change the upsampling filter size when history is rejected that reduce blocky output pixels
#define AA_UPSAMPLE_ADAPTIVE_FILTERING 1

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

// Intersect ray with AABB, knowing there is an intersection.
//   Dir = Ray direction.
//   Org = Start of the ray.
//   Box = Box is at {0,0,0} with this size.
// Returns distance on line segment.
float IntersectAABB(float3 Dir, float3 Org, float3 Box)
{
	float3 RcpDir = rcp(Dir);
	float3 TNeg = (Box - Org) * RcpDir;
	float3 TPos = ((-Box) - Org) * RcpDir;
	return max(max(min(TNeg.x, TPos.x), min(TNeg.y, TPos.y)), min(TNeg.z, TPos.z));
}





[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}