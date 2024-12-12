#include "../Shadow/RealTimeShadowHelper.hlsli"

StructuredBuffer<AreaLightInstanceData> AreaInstanceData : register(t0, space1);

float4 main(PosNorTanTexInstanceOut pin) : SV_Target
{
    AreaLightInstanceData instData = AreaInstanceData[pin.InstanceIndex];
    return float4(instData.AreaLightColor, 1.0f);
}