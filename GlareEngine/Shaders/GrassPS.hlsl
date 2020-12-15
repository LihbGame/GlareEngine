struct DomainOut
{
	float4 PosH     : SV_POSITION;
	float3 PosW     : POSITION0;
};



float4 PS(DomainOut pin) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}