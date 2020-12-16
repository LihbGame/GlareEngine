struct GSOutput
{
	float4 pos : SV_POSITION;
};



float4 PS(GSOutput pin) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}