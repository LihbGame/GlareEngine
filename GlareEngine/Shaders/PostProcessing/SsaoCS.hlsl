#include "../Misc/CommonResource.hlsli"

Texture2D<float> DepthBuffer			: register(t0);

Texture2D<float> LinearDepthBuffer		: register(t1);

RWTexture2D<unorm float> SsaoResult		: register(u0);

SamplerState BiLinearClampSampler		: register(s0);

cbuffer SSAOConstBuffer					: register(b1)
{
	float4x4			Proj;
	float4x4			InvProj;
	float				Far;
	float				Near;
	float				Range;
	float				Power;
	float2				InverseDimensions;
	int					SampleCount;
}

static const uint		BLOCKSIZE			= 8;
static const uint		BORDER				= 1;
static const uint		TILE_SIZE			= BLOCKSIZE + BORDER * 2;
static const uint		LDR_SIZE			= TILE_SIZE * TILE_SIZE;

groupshared float2		TileXY[LDR_SIZE];
groupshared float		TileZ[LDR_SIZE];

[numthreads(BLOCKSIZE, BLOCKSIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const int2 TileUpperLeft = Gid.xy * BLOCKSIZE - BORDER;

	for (uint tileIndex = GI; tileIndex < LDR_SIZE; tileIndex += BLOCKSIZE * BLOCKSIZE)
	{
		const uint2 pixel		= TileUpperLeft + unflatten2D(tileIndex, TILE_SIZE);
		const float2 uv			= (pixel + 0.5f) * InverseDimensions;
		const float depth		= DepthBuffer.SampleLevel(BiLinearClampSampler, uv, 0);
		const float3 position	= Reconstruct_Position(uv, depth, InvProj);
		TileXY[tileIndex]		= position.xy;
		TileZ[tileIndex]		= position.z;
	}
	GroupMemoryBarrierWithGroupSync();


	//Reconstructing flat normals from a depth buffer can be done using two main methods :
	//	 1.Applying ddx() and ddy() functions to the reconstructed positions to compute triangles.
	//		However, this approach can result in artifacts at depth discontinuities and cannot be used in a compute shader.
	//	 2.Taking three samples from the depth buffer, reconstructing the positions, and computing triangles.
	//		While this method may still produce artifacts at discontinuities,it is slightly better.To address the remaining artifacts, 
	//		we can take four samples around the center and find the "best triangle" by only computing positions from depths that exhibit the least amount of discontinuity.

	const uint CrossDepthIndex[5] = 
	{
		flatten2D(BORDER + GTid.xy					, TILE_SIZE),	// 0: Center
		flatten2D(BORDER + GTid.xy + int2(1, 0)		, TILE_SIZE),	// 1: Right
		flatten2D(BORDER + GTid.xy + int2(-1, 0)	, TILE_SIZE),	// 2: Left
		flatten2D(BORDER + GTid.xy + int2(0, 1)		, TILE_SIZE),	// 3: Down
		flatten2D(BORDER + GTid.xy + int2(0, -1)	, TILE_SIZE),	// 4: Up
	};

	const float CenterZ = TileZ[CrossDepthIndex[0]];

	[branch]
	if (CenterZ >= Far)
	{
		SsaoResult[DTid.xy] = 1.0f;
		return;
	}

	const uint Best_Z_Horizontal	= abs(TileZ[CrossDepthIndex[1]] - CenterZ) < abs(TileZ[CrossDepthIndex[2]] - CenterZ) ? 1 : 2;
	const uint Best_Z_Vertical		= abs(TileZ[CrossDepthIndex[3]] - CenterZ) < abs(TileZ[CrossDepthIndex[4]] - CenterZ) ? 3 : 4;

	float3 P1 = 0, P2 = 0;
	if (Best_Z_Horizontal == 1 && Best_Z_Vertical == 4)
	{
		P1 = float3(TileXY[CrossDepthIndex[1]], TileZ[CrossDepthIndex[1]]);
		P2 = float3(TileXY[CrossDepthIndex[4]], TileZ[CrossDepthIndex[4]]);
	}
	else if (Best_Z_Horizontal == 1 && Best_Z_Vertical == 3)
	{
		P1 = float3(TileXY[CrossDepthIndex[3]], TileZ[CrossDepthIndex[3]]);
		P2 = float3(TileXY[CrossDepthIndex[1]], TileZ[CrossDepthIndex[1]]);
	}
	else if (Best_Z_Horizontal == 2 && Best_Z_Vertical == 4)
	{
		P1 = float3(TileXY[CrossDepthIndex[4]], TileZ[CrossDepthIndex[4]]);
		P2 = float3(TileXY[CrossDepthIndex[2]], TileZ[CrossDepthIndex[2]]);
	}
	else if (Best_Z_Horizontal == 2 && Best_Z_Vertical == 3)
	{
		P1 = float3(TileXY[CrossDepthIndex[2]], TileZ[CrossDepthIndex[2]]);
		P2 = float3(TileXY[CrossDepthIndex[3]], TileZ[CrossDepthIndex[3]]);
	}

	const float3 P0			= float3(TileXY[CrossDepthIndex[0]], TileZ[CrossDepthIndex[0]]);
	const float3 normal		= normalize(cross(P2 - P0, P1 - P0));

	//Compute a Random Tangent 
    const float3 noise			= random32(DTid.xy + gTemporalJitter * gRenderTargetSize) * 2 - 1;
	const float3 tangent		= normalize(noise - normal * dot(noise, normal));
	const float3 bitangent		= cross(normal, tangent);
	//Tangent Space (TBN)
	const float3x3 tangentSpace = float3x3(tangent, bitangent, normal);

	float AO = 0;
	for (uint i = 0; i < SampleCount; ++i)
	{
		const float2 hammersley		= Hammersley(i, SampleCount);
		const float3 hemisphere		= HemispherePoint_Uniform(hammersley.x, hammersley.y);
		const float3 cone			= mul(hemisphere, tangentSpace);
		const float ray_range		= Range*lerp(0.2f, 1.0f, random11(i)); // avoid uniform look
		const float3 samplePoint	= P0 + cone * ray_range;

		float4 ProjectedCoord		= mul(float4(samplePoint, 1.0f), Proj);
		ProjectedCoord.xyz			/= ProjectedCoord.w;
		ProjectedCoord.xy			= ProjectedCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

		[branch]
		if (Is_Saturated(ProjectedCoord.xy))
		{
			const float Ray_Depth_Real		= ProjectedCoord.w; // .w is also linear depth
			const float Ray_Depth_Sample	= LinearDepthBuffer.SampleLevel(BiLinearClampSampler, ProjectedCoord.xy, 0) * Far;
			const float Depth_Range			= abs(Ray_Depth_Real - Ray_Depth_Sample);
			const float Depth_fix			= 1 - saturate(Depth_Range *0.2); // too much depth difference cancels the effect
			
			if (Depth_Range > 0.04f)// avoid self occlusion
			{
				AO += (Ray_Depth_Sample < Ray_Depth_Real) * Depth_fix;
			}
		}
	}
	AO /= SampleCount;

	SsaoResult[DTid.xy] = pow(saturate(1 - AO), Power);
}