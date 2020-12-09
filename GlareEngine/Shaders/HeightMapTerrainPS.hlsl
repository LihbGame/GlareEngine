#include "Common.hlsli"
#include "TerrainConstBuffer.hlsli"
float4 PS(DomainOut DomainIn) : SV_TARGET
{
	float4 diffuseAlbedo = gSRVMap[33].Sample(gsamAnisotropicWrap, DomainIn.TiledTex);
	return diffuseAlbedo;
}