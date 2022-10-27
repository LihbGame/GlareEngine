#pragma once
#include <vector>
#include "Graphics/GPUBuffer.h"
#include "Math/VectorMath.h"
#include "Engine/Camera.h"
#include "Graphics/CommandContext.h"
#include "Graphics/UploadBuffer.h"
#include "Graphics/TextureManager.h"

//Render Config 
#define NUMFRAME 3
#define MAX2DSRVSIZE 256
#define MAXCUBESRVSIZE 32
#define SHADOWMAPSIZE 1024
#define FAR_Z 5000.0f
#define CAMERA_SPEED 100.0f
#define REVERSE_Z 1

namespace GlareEngine
{
	class RootSignature;
	class DescriptorHeap;
	class GraphicsPSO;
	struct PSOCommonProperty;

	namespace Render
	{
		//Root Signature
		extern RootSignature gRootSignature;
		//PSO Common Property
		extern PSOCommonProperty gCommonProperty;
		//Num Frame Resources
		extern const int gNumFrameResources;

		extern std::vector<GraphicsPSO> gCommonPSOs;

		extern DescriptorHeap gModelTextureHeap;

		extern DescriptorHeap gSamplerHeap;

		extern BoolVar SeparateZPass;
	}


	namespace Render
	{
		//Initialize Engine Render
		void Initialize();
		void ShutDown();

		uint8_t GetPSO(uint16_t psoFlags);
		void BuildRootSignature();
		void BuildPSOs();
		void BuildCommonPSOs(const PSOCommonProperty CommonProperty);
	}
}
