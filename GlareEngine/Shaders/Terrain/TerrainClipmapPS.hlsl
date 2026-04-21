#include "TerrainCommon.hlsli"

struct PBRParams
{
    float3  Albedo;
    float3  Normal;
    float   Roughness;
    float   Metallic;
    float   AO;
};

PBRParams BlendMaterialLayers(float4 weights, float2 tiledUV, float2 worldXZ)
{
    PBRParams result = (PBRParams)0;
    float w[5];
    w[0] = weights.r; // lightdirt
    w[1] = weights.g; // darkdirt
    w[2] = weights.b; // grass
    w[3] = weights.a; // stone
    w[4] = max(0, 1.0 - w[0] - w[1] - w[2] - w[3]); // snow

    // Early-out for dominant layer
    [unroll]
    for (int i = 0; i < TERRAIN_NUM_LAYERS; i++)
    {
        if (w[i] < 0.01) continue;

        float3 albedo = SampleStochastic(gTerrainLayerAlbedo[i], tiledUV, worldXZ);
        float3 normalSample = SampleStochasticNormal(gTerrainLayerNormal[i], tiledUV, worldXZ);
        float roughness = SampleStochasticScalar(gTerrainLayerRoughness[i], tiledUV, worldXZ);
        float metallic = SampleStochasticScalar(gTerrainLayerMetallic[i], tiledUV, worldXZ);
        float ao = SampleStochasticScalar(gTerrainLayerAO[i], tiledUV, worldXZ);

        result.Albedo += albedo * w[i];
        result.Normal += normalSample * w[i];
        result.Roughness += roughness * w[i];
        result.Metallic += metallic * w[i];
        result.AO += ao * w[i];
    }

    return result;
}

float4 main(ClipmapDomainOut pin) : SV_TARGET
{
    // Tiled UV for material textures
    float2 tiledUV = pin.WorldXZ / gTerrainTexScale;

    // Blend all material layers with stochastic sampling
    PBRParams mat = BlendMaterialLayers(pin.MatWeights, tiledUV, pin.WorldXZ);

    // Decode normal from blended normal map sample
    float3 normalT = 2.0 * mat.Normal - 1.0;
    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    float3 bumpedNormalW = normalize(mul(normalT, TBN));

    // Build surface properties for PBR lighting
    SurfaceProperties surf;
    surf.N = bumpedNormalW;
    surf.V = normalize(gTerrainEyePosW - pin.PosW);
    surf.c_diff = mat.Albedo;
    surf.c_spec = lerp(DielectricSpecular, mat.Albedo, mat.Metallic);
    surf.roughness = mat.Roughness;
    surf.alpha = mat.Roughness * mat.Roughness;
    surf.alphaSqr = surf.alpha * surf.alpha;
    surf.NdotV = max(dot(surf.N, surf.V), 0.001);
    surf.worldPos = pin.PosW;
    surf.ShadowFactor = 1.0; // placeholder until shadow integration
    surf.ao = mat.AO;

    // Ambient contribution
    float3 ambient = gAmbientLight.xyz * mat.Albedo * mat.AO;

    // Direct lighting (iterate directional lights)
    float3 directLighting = float3(0, 0, 0);
    [unroll]
    for (int i = 0; i < gDirectionalLightsCount && i < MAX_DIR_LIGHTS; i++)
    {
        LightProperties light;
        light.Strength = gLights[i].Strength;
        light.Direction = gLights[i].Direction;
        light.L = normalize(-light.Direction);
        light.H = normalize(surf.V + light.L);
        light.NdotL = max(dot(surf.N, light.L), 0.0);
        light.LdotH = max(dot(light.L, light.H), 0.0);
        light.NdotH = max(dot(surf.N, light.H), 0.0);

        // Diffuse (Lambertian)
        float3 diffuse = surf.c_diff / PI;

        // Specular (GGX)
        float D = surf.alphaSqr / (PI * pow(surf.alphaSqr + light.NdotH * light.NdotH * (1.0 - surf.alphaSqr), 2.0));
        float k = (surf.roughness + 1.0) * (surf.roughness + 1.0) / 8.0;
        float G_V = surf.NdotV / (surf.NdotV * (1.0 - k) + k);
        float G_L = light.NdotL / (light.NdotL * (1.0 - k) + k);
        float G = G_V * G_L;
        float3 F = surf.c_spec + (1.0 - surf.c_spec) * pow(1.0 - light.LdotH, 5.0);

        float3 specular = (D * G * F) / (4.0 * surf.NdotV * light.NdotL + 0.001);

        directLighting += light.Strength * (diffuse + specular) * light.NdotL;
    }

    float3 finalColor = ambient + directLighting * surf.ShadowFactor;

    // Simple fog (distance-based)
    float dist = length(gTerrainEyePosW - pin.PosW);
    float fogFactor = saturate((dist - 200.0) / 2000.0);
    finalColor = lerp(finalColor, gAmbientLight.xyz * 0.5, fogFactor);

    return float4(finalColor, 1.0);
}
