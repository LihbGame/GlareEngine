#include "../VelocityPacking.hlsli"


Texture2D<float3>				ColorBuffer			: register(t0);

Texture2D<Packed_Velocity_Type> VelocityBuffer		: register(t1);

RWTexture2D<float4>				PrepBuffer			: register(u0);


#define SPEED_SCALE  8.0f


float4 GetSampleData(uint2 sp)
{
    float Speed = length(UnpackVelocity(VelocityBuffer[sp]).xy);
    return float4(ColorBuffer[sp], 1.0) * saturate(Speed * SPEED_SCALE);
}

[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    uint2 Corner = DTid.xy << 1;
    float4 Sample0 = GetSampleData(Corner + uint2(0, 0));
    float4 Sample1 = GetSampleData(Corner + uint2(1, 0));
    float4 Sample2 = GetSampleData(Corner + uint2(0, 1));
    float4 Sample3 = GetSampleData(Corner + uint2(1, 1));

    float MotionWeight = Sample0.a + Sample1.a + Sample2.a + Sample3.a + 0.0001;

    PrepBuffer[DTid.xy] = floor(0.25f * MotionWeight * 3.0f) / 3.0f *
        float4((Sample0.rgb + Sample1.rgb + Sample2.rgb + Sample3.rgb) / MotionWeight, 1.0);
}