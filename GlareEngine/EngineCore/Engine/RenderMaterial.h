#pragma once
#include "Graphics/Render.h"
#include "Graphics/PipelineState.h"


namespace GlareEngine
{
	enum MaterialShaderType :int
	{
		DefaultLight,
		MaterialTypeCount
	};


	class RenderMaterial
	{
	public:
		RenderMaterial();

		void BeginInitializeComputeMaterial(wstring MaterialName, const RootSignature& rootSignature);
		void EndInitializeComputeMaterial() { mComputePSO.Finalize(); }
		void SetComputeShader(const unsigned char* shaderByteCode, size_t byteCodeLength) { mComputePSO.SetComputeShader(shaderByteCode, byteCodeLength); }

		void BeginInitializeGraphicMaterial(wstring MaterialName, const RootSignature& rootSignature);
		void EndInitializeGraphicMaterial() { mGraphicsPSO.Finalize(); }

		void SetMaterialType(MaterialShaderType type) { mMaterialType = type; }

		ComputePSO& GetComputePSO() { return mComputePSO; }
		GraphicsPSO& GetGraphicsPSO() { return mGraphicsPSO; }
	private:
		ComputePSO mComputePSO;
		GraphicsPSO mGraphicsPSO;

		//Material Parameters
		MaterialShaderType mMaterialType = MaterialShaderType::DefaultLight;
	};
}
