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

		wstring GetMaterialName() { return mMaterialName; }

		const RootSignature* GetRootSignature() { assert(m_pRootSignature); return m_pRootSignature; }

		void BuildMaterialPSO(const PSOCommonProperty CommonProperty);

		void BindPSOCreateFunc(const std::function<void(const PSOCommonProperty)>& lambda) { mFuntionPSO = lambda; };

		ComputePSO& GetComputePSO() { return mComputePSO; }
		GraphicsPSO& GetGraphicsPSO() { return mGraphicsPSO; }
	private:
		ComputePSO mComputePSO;
		GraphicsPSO mGraphicsPSO;

		wstring mMaterialName;

		std::function<void(const PSOCommonProperty)> mFuntionPSO;

		const RootSignature* m_pRootSignature = nullptr;
		//Material Parameters
		MaterialShaderType mMaterialType = MaterialShaderType::DefaultLight;
	};


	class RenderMaterialManager
	{
	public:
		static RenderMaterialManager& GetInstance()
		{
			static RenderMaterialManager renderMaterialManager;
			return renderMaterialManager;
		}

		RenderMaterial* GetMaterial(string MaterialName);

		void BuildMaterialsPSO(const PSOCommonProperty CommonProperty);
	private:
		RenderMaterialManager() {};
		unordered_map<string, RenderMaterial> mRenderMaterialMap;
	};
}
