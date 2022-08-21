#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <vector>

#include "Animation/MeshStruct.h"
#include "Animation/ModelMesh.h"

using namespace GlareEngine;

class GeometryGenerator
{
public:
	///<summary>
	/// Creates a box centered at the origin with the given dimensions, where each
    /// face has m rows and n columns of vertices.
	///</summary>
    MeshData CreateBox(float width, float height, float depth, uint32 numSubdivisions, Color cl = { 1.0f,1.0f,1.0f,0.0f });

	///<summary>
	/// Creates a sphere centered at the origin with the given radius.  The
	/// slices and stacks parameters control the degree of tessellation.
	///</summary>
    MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount, Color cl = { 1.0f,1.0f,1.0f,0.0f });

	///<summary>
	/// Creates a geosphere centered at the origin with the given radius.  The
	/// depth controls the level of tessellation.
	///</summary>
    MeshData CreateGeosphere(float radius, uint32 numSubdivisions, Color cl = { 1.0f,1.0f,1.0f,0.0f });

	///<summary>
	/// Creates a cylinder parallel to the y-axis, and centered about the origin.  
	/// The bottom and top radius can vary to form various cone shapes rather than true
	// cylinders.  The slices and stacks parameters control the degree of tessellation.
	///</summary>
    MeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, Color cl = { 1.0f,1.0f,1.0f,0.0f });

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
    MeshData CreateGrid(float width, float depth, uint32 m, uint32 n);

	///<summary>
	/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
	///</summary>
    MeshData CreateQuad(float x, float y, float w, float h, float depth, Color cl = { 1.0f,1.0f,1.0f,0.0f });

private:
	void Subdivide(MeshData& meshData);
	Vertices::PosColorNormalTangentTex MidPoint(const Vertices::PosColorNormalTangentTex& v0, const Vertices::PosColorNormalTangentTex& v1);
    void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData);
    void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData);
};

