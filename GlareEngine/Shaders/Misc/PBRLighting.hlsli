#include "../Lighting/LightGrid.hlsli"
#include "../Misc/CommonResource.hlsli"

struct AreaLightData
{
    float3 PositionWS[4];
    float3 PositionVS[4];
    float3 PositionCenter;
    float3 LightDir;
    float3 Color;
    float RadiusSquare;
};

Texture2D<float> gSsaoTex                                           : register(t6);
StructuredBuffer<uint> gLightGridData                               : register(t7);
StructuredBuffer<TileLightData> gLightBuffer                        : register(t8);
Texture2DArray<float> gLightShadowArrayTex                          : register(t9);
//lighting cluster data             
StructuredBuffer<LightGrid> LightGridList                           : register(t10);
StructuredBuffer<uint> GlobalLightIndexList                         : register(t11);

StructuredBuffer<AreaLightData> GlobalRectAreaLightData             : register(t12);

static const float LUT_SIZE  = 64.0;
static const float LUT_SCALE = (LUT_SIZE - 1.0)/LUT_SIZE;
static const float LUT_BIAS  = 0.5/LUT_SIZE;

float3 IntegrateEdgeVec(float3 v1, float3 v2)
{
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206*y)*y;
    float b = 3.4175940 + (4.1616724 + y)*y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5*rsqrt(max(1.0 - x*x, 1e-7)) - v;

    return cross(v1, v2)*theta_sintheta;
}

float IntegrateEdge(float3 v1, float3 v2)
{
    return IntegrateEdgeVec(v1, v2).z;
}


//BRDF-F
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float InvRoughness = 1.0f - roughness;
    return F0 + (max(float3(InvRoughness, InvRoughness, InvRoughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

//BRDF-D
// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0 + EPS);
    denom = PI * denom * denom;

    return nom / denom;
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    double a = pow(roughness, 4);
    double cosS = (1.0 - Xi.y) / (1.0 + (a - 1.0) * Xi.y);
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(float(cosS));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}



float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

//BRDF-G
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}


float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    // Linear falloff.
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

//Disney F_Schlick 
float3 F_Schlick(in float3 f0, in float f90, in float u)
{
 return f0 + ( f90 - f0 ) * pow(1.0f - u , 5.0f);
}

float Fresnel_Shlick(float F0, float F90, float cosine)
{
    return lerp(F0, F90, pow(1.0 - cosine, 5.0f));
}

//Disney diffuse BRDF code with renormalization of its energy(Burley's diffuse BRDF)
float Fd_Disney(float NdotV, float NdotL, float LdotH,float linearRoughness)
{
    float energyFactor = lerp(1.0, 1.0 / 1.51, linearRoughness);
    float fd90 = 0.5 + 2.0 * LdotH * LdotH * linearRoughness;
    float3 f0 = float3(1.0f, 1.0f, 1.0f);
    float lightScatter = F_Schlick(f0, fd90, NdotL).r;
    float viewScatter = F_Schlick(f0, fd90, NdotV).r;
    return lightScatter * viewScatter * energyFactor / PI;
}

float Fd_Lambert()
{
    return 1.0 / PI;
}

//BlinnPhong
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = (1 - mat.Roughness) * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    // Our spec formula goes outside [0,1] range, but we are 
    // doing LDR rendering.  So scale it down a bit.
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

// cook-torrance BRDF
float3 CookTorranceBRDF(float3 radiance, float3 N, float3 H, float3 V, float3 L, float3 F0, Material mat)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float LdotH = saturate(dot(L, H));
   
    
    float NDF = DistributionGGX(N, H, mat.Roughness);
    float G = GeometrySmith(N, V, L, mat.Roughness);
    float3 F = fresnelSchlick(saturate(dot(H, V)), F0);

    float3 kS = F;
    //float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    float3 kD = Fd_Disney(NdotV, NdotL, LdotH, mat.Roughness);
    kD *= (1.0 - mat.metallic);

    float3 nominator = NDF * G *F;
    float denominator = 4.0 * NdotV * NdotL + 0.001;
    float3 specular = nominator / denominator;

    // outgoing radiance Lo
    return (kD * mat.DiffuseAlbedo.rgb + specular) * radiance * NdotL;

}


//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(DirectionalLight L, Material mat, float3 normal, float3 toEye, float3 F0)
{
    // The light vector aims opposite the direction the light rays travel.
    float3 lightVec = -L.Direction;

    float3 lightStrength = L.Strength;

    float3 Half = normalize(toEye + lightVec);

    return CookTorranceBRDF(lightStrength, normal, Half, toEye, lightVec, F0, mat);
}

////---------------------------------------------------------------------------------------
//// Evaluates the lighting equation for point lights.
////---------------------------------------------------------------------------------------
//float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye, float3 F0)
//{
//    // The vector from the surface to the light.
//    float3 lightVec = L.Position - pos;
//
//    // The distance from surface to light.
//    float d = length(lightVec);
//
//    // Range test.
//    if (d > L.FalloffEnd)
//        return 0.0f;
//
//    // Normalize the light vector.
//    lightVec /= d;
//
//    // Scale light down by Lambert's cosine law.
//    float ndotl = max(dot(lightVec, normal), 0.0f);
//    float3 lightStrength = L.Strength * ndotl;
//
//    // Attenuate light by distance.
//    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
//    lightStrength *= att;
//
//
//    float3 Half = normalize(toEye + lightVec);
//
//    return CookTorranceBRDF(lightStrength, normal, Half, toEye, lightVec, F0, mat);
//}
//
////---------------------------------------------------------------------------------------
//// Evaluates the lighting equation for spot lights.
////---------------------------------------------------------------------------------------
//float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye, float3 F0)
//{
//    // The vector from the surface to the light.
//    float3 lightVec = L.Position - pos;
//
//    // The distance from surface to light.
//    float d = length(lightVec);
//
//    // Range test.
//    if (d > L.FalloffEnd)
//        return 0.0f;
//
//    // Normalize the light vector.
//    lightVec /= d;
//
//    // Scale light down by Lambert's cosine law.
//    float ndotl = max(dot(lightVec, normal), 0.0f);
//    float3 lightStrength = L.Strength * ndotl;
//
//    // Attenuate light by distance.
//    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
//    lightStrength *= att;
//
//    // Scale by spotlight
//    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
//    lightStrength *= spotFactor;
//
//    float3 Half = normalize(toEye + lightVec);
//
//    return CookTorranceBRDF(lightStrength, normal, Half, toEye, lightVec, F0, mat);
//}


float4 ComputeLighting(DirectionalLight gLights[MAX_DIR_LIGHTS], Material mat,
                       float3 pos, float3 normal, float3 toEye,
                       float3 shadowFactor)
{
    float3 result = 0.0f;

    int i = 0;

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, mat.DiffuseAlbedo.rgb, mat.metallic);

    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        gLights[i].Strength *= shadowFactor[0];
        result += ComputeDirectionalLight(gLights[i], mat, normal, toEye, F0);
    }

    return float4(result, 0.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

float3 CookTorranceBRDF(in LightProperties LightProper, in SurfaceProperties Surface)
{
    float NDF = DistributionGGX(Surface.N, LightProper.H, Surface.roughness);
    float G = GeometrySmith(Surface.N, Surface.V, LightProper.L, Surface.roughness);
    float3 F = fresnelSchlick(saturate(dot(LightProper.H, Surface.V)), Surface.c_spec);

    //float3 kS = F;
    //float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    float3 kD = Fd_Disney(Surface.NdotV, LightProper.NdotL, LightProper.LdotH, Surface.roughness);

    float3 nominator = NDF * G * F;
    float denominator = 4.0 * Surface.NdotV * LightProper.NdotL + 0.001f;
    float3 specular = nominator / denominator;

    // outgoing radiance Lo
    return (kD * Surface.c_diff  + specular) * LightProper.Strength * LightProper.NdotL;
}


LightProperties GetLightProperties(in DirectionalLight light, in SurfaceProperties Surface)
{
    LightProperties LightProp;
    LightProp.L = normalize(-light.Direction);

    // Half vector
    LightProp.H = normalize(LightProp.L + Surface.V);

    // Pre-compute dot products
    LightProp.NdotL = saturate(dot(Surface.N, LightProp.L));
    LightProp.LdotH = saturate(dot(LightProp.L, LightProp.H));
    LightProp.NdotH = saturate(dot(Surface.N, LightProp.H));
    LightProp.Strength = light.Strength;
    LightProp.Direction = light.Direction;
    return LightProp;
}


float3 ComputeDirectionalLight(in LightProperties LightProper, in SurfaceProperties Surface)
{
    return CookTorranceBRDF(LightProper, Surface);
}

float3 ComputeLighting(in DirectionalLight lights[MAX_DIR_LIGHTS], in SurfaceProperties Surface)
{
    float3 LightResult = float3(0, 0, 0);

    int i = 0;
    
    if (gDirectionalLightsCount > 0 && gEnableDirectionalLight)
    {
        for (i = 0; i < gDirectionalLightsCount; ++i)
        {
            lights[i].Strength *= smoothstep(0.0f, 1.0f, Surface.ShadowFactor);
            LightProperties lightProperties = GetLightProperties(lights[i], Surface);
            LightResult += ComputeDirectionalLight(lightProperties, Surface);
        }
    }
    return  LightResult;
}

float3 ComputePointLight(in TileLightData LightData, in SurfaceProperties Surface)
{
    LightProperties LightProp;
    LightProp.L = normalize(LightData.PositionWS - Surface.worldPos);

    // Half vector
    LightProp.H = normalize(LightProp.L + Surface.V);

    // Pre-compute dot products
    LightProp.NdotL = saturate(dot(Surface.N, LightProp.L));
    LightProp.LdotH = saturate(dot(LightProp.L, LightProp.H));
    LightProp.NdotH = saturate(dot(Surface.N, LightProp.H));
    LightProp.Strength = LightData.Color;


    float3 lightDir = LightData.PositionWS - Surface.worldPos;
    float lightDistSq = dot(lightDir, lightDir);
    float invLightDist = rsqrt(lightDistSq);


    // modify 1/d^2 * R^2 to fall off at a fixed radius
    // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
    float distanceFalloff = LightData.RadiusSq * (invLightDist * invLightDist);
    distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

    return distanceFalloff * CookTorranceBRDF(LightProp, Surface);
}

float3 ComputeConeLight(in TileLightData LightData, in SurfaceProperties Surface)
{
    LightProperties LightProp;
    LightProp.L = normalize(LightData.PositionWS - Surface.worldPos);

    // Half vector
    LightProp.H = normalize(LightProp.L + Surface.V);

    // Pre-compute dot products
    LightProp.NdotL = saturate(dot(Surface.N, LightProp.L));
    LightProp.LdotH = saturate(dot(LightProp.L, LightProp.H));
    LightProp.NdotH = saturate(dot(Surface.N, LightProp.H));
    LightProp.Strength = LightData.Color;

    float3 lightDir = LightData.PositionWS - Surface.worldPos;
    float lightDistSq = dot(lightDir, lightDir);
    float invLightDist = rsqrt(lightDistSq);

    // modify 1/d^2 * R^2 to fall off at a fixed radius
    // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
    float distanceFalloff = LightData.RadiusSq * (invLightDist * invLightDist);
    distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));

    float coneFalloff = dot(-LightProp.L, LightData.ConeDir);
    coneFalloff = saturate((coneFalloff - LightData.ConeAngles.y) * LightData.ConeAngles.x);

    return coneFalloff * distanceFalloff * CookTorranceBRDF(LightProp, Surface);
}

float GetShadowConeLight(uint lightIndex, float3 shadowCoord)
{
    float result = gLightShadowArrayTex.SampleCmpLevelZero(
        gSamplerShadow, float3(shadowCoord.xy, lightIndex), shadowCoord.z);
    return result;
}

float3 ComputeConeShadowLight(in TileLightData LightData, in SurfaceProperties Surface)
{
    float3 lightColor = ComputeConeLight(LightData, Surface);
    float4 shadowCoord = mul(LightData.ShadowTextureMatrix, float4(Surface.worldPos, 1.0));
    shadowCoord.xyz *= rcp(shadowCoord.w);
    float shadowFactor = GetShadowConeLight(LightData.ShadowConeIndex, shadowCoord.xyz);
    return lightColor* shadowFactor;
}

// Linearly Transformed Cosines
float3 LTC_Evaluate(float3 N, float3 V, float3 P, float3x3 ltcMat, float3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
    float3 T1, T2;
    T1 = normalize(V - N*dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    ltcMat = mul(transpose(float3x3(T1, T2, N)), ltcMat);

    // polygon (allocate 5 vertices for clipping)
    float3 L[5];
    L[0] = mul(points[0] - P,ltcMat);
    L[1] = mul(points[1] - P,ltcMat);
    L[2] = mul(points[2] - P,ltcMat);
    L[3] = mul(points[3] - P,ltcMat);

    // integrate
    float sum = 0.0;

    float3 dir = points[0].xyz - P;
    float3 lightNormal = cross(points[1] - points[0],points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);
    
    if (behind && !twoSided)
    {
        return float3(0, 0, 0);
    }
        
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    
    float3 vsum = float3(0.0,0.0,0.0);
    
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);
    
    float len = length(vsum);
    float z = vsum.z/len;
    
    if (behind)
        z = -z;
    
    float2 uv = float2(z * 0.5 + 0.5, len);
    uv = uv*LUT_SCALE + LUT_BIAS;
    
    float scale = gSRVMap[gAreaLightLTC2SRVIndex].SampleLevel(gSamplerLinearClamp, uv, 0).w;
    
    sum = len * scale;
    
    if (behind && !twoSided)
        sum = 0.0;
    
    return float3(sum, sum, sum);
}

// Area lighting calculation
float3 ComputeAreaLighting(in AreaLightData lightData, in SurfaceProperties Surface)
{
    float2 UV = float2(Surface.roughness, sqrt(1.0-Surface.NdotV));
    UV=UV*LUT_SCALE+LUT_BIAS;
    
    float4 T1 = gSRVMap[gAreaLightLTC1SRVIndex].SampleLevel(gSamplerLinearClamp, UV, 0);
    float4 T2 = gSRVMap[gAreaLightLTC2SRVIndex].SampleLevel(gSamplerLinearClamp, UV, 0);

    float3x3 ltcMat= float3x3(
        float3(T1.x,0,T1.y),
        float3(0,1,0),
        float3(T1.z,0,T1.w));
    
    float3 specular = LTC_Evaluate(Surface.N, Surface.V, Surface.worldPos, ltcMat, lightData.PositionWS, gAreaLightTwoSide);
    float3 diffuse = LTC_Evaluate(Surface.N, Surface.V, Surface.worldPos, IdentityMatrix3x3, lightData.PositionWS, gAreaLightTwoSide);

    // GGX BRDF shadowing and Fresnel
    // T2.x: shadowedF90 (F90 normally it should be 1.0)
    // T2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
    specular *= Surface.c_spec*T2.x + (1.0f - Surface.c_spec) * T2.y;

    float3 lightDir = lightData.PositionCenter - Surface.worldPos;
    float lightDistSq = dot(lightDir, lightDir);
    float invLightDist = rsqrt(lightDistSq);
    // modify 1/d^2 * R^2 to fall off at a fixed radius
    // (R/d)^2 - d/R = [(1/d^2) - (1/R^2)*(d/R)] * R^2
    float distanceFalloff = lightData.RadiusSquare * (invLightDist * invLightDist);
    distanceFalloff = max(0, distanceFalloff - rsqrt(distanceFalloff));
    
    float3 color = distanceFalloff * lightData.Color * (specular + Surface.c_diff * diffuse);
    
    return color;
    
}

float3 ComputeLighting_Internal(uint LightCount, uint LightOffset, in SurfaceProperties Surface)
{
    float3 lightColor = float3(0, 0, 0);

    for (uint Index = 0; Index < LightCount; Index++, LightOffset += 1)
    {
        uint lightIndex = 0;
        
        if (gIsClusterBaseLighting)
        {
            lightIndex = GlobalLightIndexList[LightOffset];
        }
        else
        {
            lightIndex = gLightGridData[LightOffset];
        }
        
        TileLightData lightData = gLightBuffer[lightIndex];

        switch (lightData.Type)
        {
        case 0: //Point light
            {
                if (gEnablePointLight)
                {
                    lightColor += ComputePointLight(lightData, Surface);
                }
                break;
            }
        case 1: //Cone Light
            {
                if (gEnableConeLight)
                {
                    lightColor += ComputeConeLight(lightData, Surface);
                }
                break;
            }
        case 2: //Shadowed Cone Light 
            {
                if (gEnableConeLight)
                {
                    lightColor += ComputeConeShadowLight(lightData, Surface);
                }
                break;
            }
        case 3: //Area Light 
            {
                if (gEnableAreaLight)
                {
                    lightColor +=ComputeAreaLighting(GlobalRectAreaLightData[lightData.AreaLightIndex], Surface);
                }
                break;
            }
        default:
            break;
        }
    }
    return lightColor;
}

float3 ComputeTiledLighting(uint2 ScreenPosition, in SurfaceProperties Surface)
{
    uint2 tilePos = GetTilePos(ScreenPosition, gInvTileDimension.xy);
    uint tileIndex = GetTileIndex(tilePos, gTileCount.x);
    uint tileOffset = GetTileOffset(tileIndex);

    uint tileLightCount = gLightGridData[tileOffset];
    uint tileLightLoadOffset = tileOffset + 1;

    return ComputeLighting_Internal(tileLightCount, tileLightLoadOffset, Surface);

}

float3 ComputeClusterLighting(uint2 ScreenPosition, in float viewZ, in SurfaceProperties Surface)
{
    uint clusterZ = uint(max(log2(viewZ) * gCluserFactor.x + gCluserFactor.y, 0.0));
    uint3 clusters = uint3(uint(ScreenPosition.x / gPerTileSize.x), uint(ScreenPosition.y / gPerTileSize.y), clusterZ);
    uint clusterIndex = clusters.x + clusters.y * (uint) gTileSizes.x + clusters.z * (uint) gTileSizes.x * (uint) gTileSizes.y;
    LightGrid lightGrid = LightGridList[clusterIndex];
    return ComputeLighting_Internal(lightGrid.count, lightGrid.offset, Surface);
}


float3 IBL_Diffuse(SurfaceProperties Surface)
{
    // This is nicer but more expensive
    float LdotH = saturate(dot(Surface.N, normalize(Surface.N + Surface.V)));
    float fd90 = 0.5 + 2.0 * Surface.roughness * LdotH * LdotH;
    float3 DiffuseBurley = Surface.c_diff * Surface.ao * (1 - DielectricSpecular) * Fresnel_Shlick(1, fd90, Surface.NdotV);
    return DiffuseBurley * gCubeMaps[gBakingDiffuseCubeIndex].SampleLevel(gSamplerLinearWrap, Surface.N, 0).rgb;
}

// Approximate specular IBL by sampling lower mips according to roughness.
float3 IBL_Specular(SurfaceProperties Surface)
{
    float lod = Surface.roughness * IBLRange + IBLBias;
    float3 F = FresnelSchlickRoughness(Surface.NdotV, Surface.c_spec, Surface.roughness);
    float3 PrefilteredColor = gCubeMaps[gBakingPreFilteredEnvIndex].SampleLevel(gSamplerLinearClamp, reflect(-Surface.V, Surface.N), lod).rgb;
    float2 BRDF = gSRVMap[gBakingIntegrationBRDFIndex].SampleLevel(gSamplerLinearClamp, float2(Surface.NdotV, Surface.roughness), 0).xy;
    return PrefilteredColor * (F * BRDF.x + BRDF.y) * Surface.ao;
}





