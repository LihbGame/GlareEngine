#include "../Misc/CommonResource.hlsli"


//Height map terrain functions
float CalcTessFactor(float3 p)
{
	float d = distance(p, gEyePosW);

	float s = saturate((d - gMinDist) / (gMaxDist - gMinDist));
	s = ceil(lerp(gMaxTess, gMinTess, s));
	return pow(2, s);
}


PatchTess CalcHSPatchConstants(
	InputPatch<VertexOut, NUM_CONTROL_POINTS> patch,
	uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	//
	// Frustum cull
	//

	// We store the patch BoundsY in the first control point.
	float minY = patch[0].BoundsY.x;
	float maxY = patch[0].BoundsY.y;

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
	else//tessellation based on distance.
	{
		//It is important that the tessellation coefficient calculation is done based on edge properties. 
		//so that edges shared by more than one patch have the same subdivision factor. 
		// Otherwise gaps may appear.
		
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
	};
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("CalcHSPatchConstants")]
[maxtessfactor(64.0f)]
HullOut main(InputPatch<VertexOut, NUM_CONTROL_POINTS> ip,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HullOut hout;

	// Pass through shader.
	hout.PosW = ip[i].PosW;
	hout.Tex = ip[i].Tex;

	return hout;
}

