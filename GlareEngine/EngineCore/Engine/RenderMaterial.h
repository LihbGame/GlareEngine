#pragma once
#include "Graphics/Render.h"
#include "Graphics/PipelineState.h"
#include "Tools/RuntimePSOManager.h"


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

	enum MaterialPipelineType :int
	{
		Graphics,
		Compute
	};



	class RenderMaterial
	{
	public:
		RenderMaterial(wstring MaterialName = L"Default Material Name", MaterialPipelineType PipelineType = MaterialPipelineType::Graphics);

		void BeginInitializeComputeMaterial(const RootSignature& rootSignature);
		void EndInitializeComputeMaterial() { mComputePSO.Finalize(); }
		void SetComputeShader(const unsigned char* shaderByteCode, size_t byteCodeLength) { mComputePSO.SetComputeShader(shaderByteCode, byteCodeLength); }

		void SetMaterialType(MaterialShaderType type) { mMaterialType = type; }

		wstring GetMaterialName() { return mMaterialName; }

		const RootSignature* GetRootSignature() { assert(m_pRootSignature); return m_pRootSignature; }

		void BuildMaterialPSO(const PSOCommonProperty CommonProperty);

		void BindPSOCreateFunc(const std::function<void(const PSOCommonProperty)>& lambda) { mBuildPSOFunction = lambda; }

		void InitRuntimePSO();
		void BindPSORuntimeModifyFunc(const std::function<void()>& lambda) { mRuntimeModifyPSOFunction = lambda; }

		ComputePSO& GetComputePSO() { return mComputePSO; }
		GraphicsPSO& GetGraphicsPSO() { return mGraphicsPSO; }

		PSO& GetRuntimePSO();
	public:
		bool IsInitialized = false;
	private:
		ComputePSO mComputePSO;
		GraphicsPSO mGraphicsPSO;

		MaterialPipelineType  mPipelineType;

		wstring mMaterialName;

		std::function<void(const PSOCommonProperty)> mBuildPSOFunction;
		std::function<void()> mRuntimeModifyPSOFunction;

		const RootSignature* m_pRootSignature = nullptr;
		//Material Parameters
		MaterialShaderType mMaterialType = MaterialShaderType::DefaultLight;
	};


	class RenderMaterialManager
	{
		friend class RenderMaterial;
	public:
		static RenderMaterialManager& GetInstance()
		{
			static RenderMaterialManager renderMaterialManager;
			return renderMaterialManager;
		}

		RenderMaterial* GetMaterial(string MaterialName, MaterialPipelineType PipelineType = MaterialPipelineType::Graphics);

		void BuildMaterialsPSO(const PSOCommonProperty CommonProperty);
		void InitRuntimePSO();

	private:
		RenderMaterialManager() {};
		unordered_map<string, RenderMaterial> mRenderMaterialMap;
	};
}
