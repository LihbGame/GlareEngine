
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
	int2(0, -1),
	int2(1, -1),
	int2(-1,  0),
	int2(0,  0), // center
	int2(1,  0),
	int2(-1,  1),
	int2(0,  1),
	int2(1,  1),
};





[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}