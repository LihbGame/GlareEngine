#pragma once

#include <cstdint>


class Camera;

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
		extern IntVar LightGridDim;

		enum { MaxLights = 1024 };

		extern StructuredBuffer m_LightBuffer;
		extern ByteAddressBuffer m_LightGrid;

		extern std::uint32_t m_FirstConeLight;
		extern std::uint32_t m_FirstConeShadowedLight;

		extern ColorBuffer m_LightShadowArray;
		extern ShadowBuffer m_LightShadowTempBuffer;
		extern Matrix4 m_LightShadowMatrix[MaxLights];

		void InitializeResources(void);
		void CreateRandomLights(const Vector3 minBound, const Vector3 maxBound);
		void FillLightGrid(GraphicsContext& gfxContext, const Camera& camera);
		void Shutdown(void);
	}
}
