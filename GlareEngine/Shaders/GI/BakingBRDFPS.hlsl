#include "../Misc/PBRLighting.hlsli"


float IBLGeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness;
    float k = (r * r) / 2.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

//BRDF-G
float IBLGeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = IBLGeometrySchlickGGX(NdotV, roughness);
    float ggx1 = IBLGeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}




float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    float3 N = float3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = IBLGeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}


float2 main(TexVSOut pin) : SV_TARGET
{
    float2 integratedBRDF = IntegrateBRDF(pin.Tex.x, pin.Tex.y);
    return integratedBRDF;
}