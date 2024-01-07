#pragma once

#include <cstdint>


class Camera;
struct MainConstants;

namespace GlareEngine
{
	namespace Math
	{
		class Vector3;
		class Matrix4;
	}

	class StructuredBuffer;
	class ByteAddressBuffer;
	class ColorBuffer;
	class ShadowBuffer;
	class GraphicsContext;
	class IntVar;

	namespace Lighting
	{
		using namespace GlareEngine::Math;
		extern IntVar LightGridDimension;

		enum { MaxLights = 1024 };
		enum { MaxTileLights = 512 };
		enum { MaxClusterLights = 50 };
		enum { MaxShadowedLights = 16 };

		extern StructuredBuffer m_LightBuffer;
		extern StructuredBuffer m_LightGrid;

		extern std::uint32_t m_FirstConeLight;
		extern std::uint32_t m_FirstConeShadowedLight;

		extern ShadowBuffer m_LightShadowArray;
		extern ShadowBuffer m_LightShadowTempBuffer;
		extern Matrix4 m_LightShadowMatrix[MaxShadowedLights];

		extern StructuredBuffer m_LightCluster;
		extern StructuredBuffer m_ClusterLightGrid;
		extern StructuredBuffer m_UnusedClusterMask;
		extern StructuredBuffer m_GlobalLightIndexList;
		extern StructuredBuffer m_GlobalIndexOffset;

		extern Camera ConeShadowCamera[MaxShadowedLights];

		void InitializeResources(const Camera& camera);
		void CreateRandomLights(const Vector3 minBound, const Vector3 maxBound, const Vector3 offset);
		void FillLightGrid(GraphicsContext& gfxContext, const Camera& camera);

		void BuildCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants);
		void MaskUnUsedCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants);
		void ClusterLightingCulling(GraphicsContext& gfxContext);

		void Update();
		void Shutdown(void);
	}
}

