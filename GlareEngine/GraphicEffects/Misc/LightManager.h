#pragma once

#include <cstdint>
#include "Engine/Tools/RuntimePSOManager.h"

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
	class DepthBuffer;
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
		enum { MaxAreaLights = 16 };

		enum LightType {
			PointLight, 
			ConeLight,
			ShadowedConeLight,
			AreaLight
		};
		extern bool bEnableTiledBaseLight;
		extern bool bEnablePointLight;
		extern bool bEnableAreaLight;
		extern bool bEnableConeLight;
		extern bool bEnableDirectionalLight;
		
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

		extern uint32_t AreaLightLTC1SRVIndex;
		extern uint32_t AreaLightLTC2SRVIndex;
		
		extern Camera ConeShadowCamera[MaxShadowedLights];

		void InitializeResources(const Camera& camera);
		void CreateLightRenderData();
		void CreateRandomLights(const Vector3 minBound, const Vector3 maxBound, const Vector3 offset);
		void FillLightGrid(GraphicsContext& gfxContext, const Camera& camera);
		void FillLightGrid_Internal(GraphicsContext& gfxContext, const Camera& camera,ColorBuffer& SceneColor,DepthBuffer& SceneDepth);

		void BuildCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants);
		void MaskUnUsedCluster(GraphicsContext& gfxContext, const MainConstants& mainConstants);
		void ClusterLightingCulling(GraphicsContext& gfxContext);

		void Update(MainConstants& mainConstants, Camera& camera);
		void UpdateViewSpacePosition(Camera& camera);
		void UpdateLightingDataChange(bool IsChanged);

		void RenderAreaLightMesh(GraphicsContext& context);

		void Shutdown(void);

		void InitMaterial();

		void DrawUI();

	}
}

