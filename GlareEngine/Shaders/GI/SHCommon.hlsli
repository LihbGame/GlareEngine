
struct ThreeBandSHVector
{
    float4  V0;
    float4  V1;
    float   V2;
};

struct ThreeBandSHVectorRGBF
{
    ThreeBandSHVector R;
    ThreeBandSHVector G;
    ThreeBandSHVector B;
};


ThreeBandSHVector SHBasisFunction3(float3 InputVector)
{
    ThreeBandSHVector Result;
	// These are derived from simplifying SHBasisFunction in C++
    Result.V0.x = 0.282095f;
    Result.V0.y = -0.488603f * InputVector.y;
    Result.V0.z = 0.488603f * InputVector.z;
    Result.V0.w = -0.488603f * InputVector.x;

    half3 VectorSquared = InputVector * InputVector;
    Result.V1.x = 1.092548f * InputVector.x * InputVector.y;
    Result.V1.y = -1.092548f * InputVector.y * InputVector.z;
    Result.V1.z = 0.315392f * (3.0f * VectorSquared.z - 1.0f);
    Result.V1.w = -1.092548f * InputVector.x * InputVector.z;
    Result.V2 = 0.546274f * (VectorSquared.x - VectorSquared.y);

    return Result;
}


