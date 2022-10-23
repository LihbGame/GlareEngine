#include "TerrainConstBuffer.hlsli"
#include "Common.hlsli"

struct VertexOut
{
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD;
	float2 BoundsY  : BoundY;
};

struct PatchTess
{
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
	float3 PosW     : POSITION;
	float2 Tex      : TEXCOORD;
};

//Height map terrain functions
float CalcTessFactor(float3 p)
{
	float d = distance(p, gEyePosW);

	float s = saturate((d - gMinDist) / (gMaxDist - gMinDist));
	s = ceil(lerp(gMaxTess, gMinTess, s));
	return pow(2, s);
}

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	//
	// Frustum cull
	//

	// We store the patch BoundsY in the first control point.
	float minY;
	float maxY;
	if (!isReflection)
	{
		 minY = patch[0].BoundsY.x;
		 maxY = patch[0].BoundsY.y;
	}
	else
	{
		maxY = -patch[0].BoundsY.x;
		minY = -patch[0].BoundsY.y;
	}
	// Build axis-aligned bounding box.  patch[2] is lower-left corner
	// and patch[1] is upper-right corner.
	float3 vMin = float3(patch[2].PosW.x, minY, patch[2].PosW.z);
	float3 vMax = float3(patch[1].PosW.x, maxY, patch[1].PosW.z);

	float3 boxCenter = 0.5f * (vMin + vMax);
	float3 boxExtents = 0.5f * (vMax - vMin);
	if (AABBOutsideFrustumTest(boxCenter, boxExtents, gWorldFrustumPlanes))
	{
		pt.EdgeTess[0] = 0.0f;
		pt.EdgeTess[1] = 0.0f;
		pt.EdgeTess[2] = 0.0f;
		pt.EdgeTess[3] = 0.0f;

		pt.InsideTess[0] = 0.0f;
		pt.InsideTess[1] = 0.0f;

		return pt;
	}
	else//���ݾ��������ͨ��Ƕ��
	{
		//��Ҫ���ǣ�Ҫ���ݱ�Ե���Խ�����Ƕϵ�����㣬
		//��ʹһ�����ϵ���Ƭ����ı߾�����ͬ��ϸ��ϵ����
		//������ܻ���ּ�϶��

		// Compute midpoint on edges, and patch center
		float3 e0 = 0.5f * (patch[0].PosW + patch[2].PosW);
		float3 e1 = 0.5f * (patch[0].PosW + patch[1].PosW);
		float3 e2 = 0.5f * (patch[1].PosW + patch[3].PosW);
		float3 e3 = 0.5f * (patch[2].PosW + patch[3].PosW);
		float3  c = 0.25f * (patch[0].PosW + patch[1].PosW + patch[2].PosW + patch[3].PosW);

		pt.EdgeTess[0] = CalcTessFactor(e0);
		pt.EdgeTess[1] = CalcTessFactor(e1);
		pt.EdgeTess[2] = CalcTessFactor(e2);
		pt.EdgeTess[3] = CalcTessFactor(e3);

		pt.InsideTess[0] = CalcTessFactor(c);
		pt.InsideTess[1] = pt.InsideTess[0];

		return pt;
	}
}



[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> p,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HullOut hout;

	// Pass through shader.
	hout.PosW = p[i].PosW;
	hout.Tex = p[i].Tex;

	return hout;
}

