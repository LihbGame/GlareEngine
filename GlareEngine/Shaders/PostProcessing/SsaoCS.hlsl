#include "../Misc/CommonResource.hlsli"

Texture2D<float> DepthBuffer : register(t0);

RWTexture2D<float> SsaoResult : register(u0);

SamplerState BiLinearClampSampler: register(s0);

cbuffer SSAOConstBuffer : register(b0)
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

groupshared float2		Tile_XY[LDR_SIZE];
groupshared float		Tile_Z[LDR_SIZE];

[numthreads(BLOCKSIZE, BLOCKSIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const int2 TileUpperLeft = Gid.xy * BLOCKSIZE - BORDER;

	for (uint tileIndex = GI; tileIndex < LDR_SIZE; tileIndex += BLOCKSIZE * BLOCKSIZE)
	{
		const uint2 pixel = TileUpperLeft + unflatten2D(tileIndex, TILE_SIZE);
		const float2 uv = (pixel + 0.5f) * InverseDimensions;
		const float depth = DepthBuffer.SampleLevel(BiLinearClampSampler, uv, 0);
		const float3 position = Reconstruct_Position(uv, depth, InvProj);
		Tile_XY[tileIndex] = position.xy;
		Tile_Z[tileIndex] = position.z;
	}
	GroupMemoryBarrierWithGroupSync();


}