#pragma once
#include "Graphics/Render.h"
#include "Graphics/PipelineState.h"


#define InitComputeMaterial(rootSignature,Material, ShaderByteCode) \
    Material.BeginInitializeComputeMaterial(rootSignature); \
    Material.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
	Material.EndInitializeComputeMaterial();


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
		RenderMaterial(wstring MaterialName=L"Default Material Name");

		void BeginInitializeComputeMaterial(const RootSignature& rootSignature);
		void EndInitializeComputeMaterial() { mComputePSO.Finalize(); }
		void SetComputeShader(const unsigned char* shaderByteCode, size_t byteCodeLength) { mComputePSO.SetComputeShader(shaderByteCode, byteCodeLength); }

		void BeginInitializeGraphicMaterial(const RootSignature& rootSignature);
		void EndInitializeGraphicMaterial() { mGraphicsPSO.Finalize(); }

		void SetMaterialType(MaterialShaderType type) { mMaterialType = type; }

		ComputePSO& GetComputePSO() { return mComputePSO; }
		GraphicsPSO& GetGraphicsPSO() { return mGraphicsPSO; }
	private:
		ComputePSO mComputePSO;
		GraphicsPSO mGraphicsPSO;

		wstring mMaterialName;

		//Material Parameters
		MaterialShaderType mMaterialType = MaterialShaderType::DefaultLight;
	};
}
