#pragma once
#include <vector>
#include "Graphics/GPUBuffer.h"
#include "Math/VectorMath.h"
#include "Engine/Camera.h"
#include "Graphics/CommandContext.h"
#include "Graphics/UploadBuffer.h"
#include "Graphics/TextureManager.h"
#include "Engine/EngineConfig.h"

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
		//Number Frame Resources
		extern const int gNumFrameResources;

		extern std::vector<GraphicsPSO> gCommonPSOs;

		extern DescriptorHeap gTextureHeap;

		extern DescriptorHeap gSamplerHeap;

		extern BoolVar SeparateZPass;
	}


	namespace Render
	{
		//Initialize Engine Render
		void Initialize(ID3D12GraphicsCommandList* CommandList);
		void ShutDown();

		uint8_t GetPSO(uint16_t psoFlags);
		void BuildRootSignature();
		void BuildPSOs();
		void BuildCommonPSOs(const PSOCommonProperty CommonProperty);
		void CopyTextureHeap();
	}
}
