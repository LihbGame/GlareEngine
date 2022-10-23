
struct VertexOut
{
	float4 PosH    : SV_POSITION;
};


float4 PS(VertexOut input) : SV_Target
{
	return float4(0.0f,0.0f,0.0f,0.0f);
}