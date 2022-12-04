#include "Sky.h"
#include "Engine/GeometryGenerator.h"
#include "Graphics/TextureManager.h"
#include "Misc/ConstantBuffer.h"
#include "EngineGUI.h"

//shader
#include "CompiledShaders/SkyVS.h"
#include "CompiledShaders/SkyPS.h"

GraphicsPSO CSky::mPSO;

#define  HDR_SKY L"HDRSky\\SKY_HDR"
#define  LDR_SKY L"HDRSky\\SKY_LDR"


CSky::CSky(ID3D12GraphicsCommandList* CommandList,
	float radius, int sliceCount, int stackCount)
{
	SetName(L"Sky");
	mObjectType = ObjectType::Sky;
	BuildSkyMesh(CommandList, radius, sliceCount, stackCount);
	BuildSkySRV(CommandList);
	//world mat
	XMStoreFloat4x4(&mWorld, XMMatrixScaling(50.0f, 50.0f, 50.0f));
}

CSky::~CSky()
{
}


void CSky::BuildSkyMesh(ID3D12GraphicsCommandList* CommandList, float radius, int sliceCount, int stackCount)
{
	GeometryGenerator SphereGeo;
	MeshData Sphere = SphereGeo.CreateSphere(radius, sliceCount, stackCount);

	std::vector<Vertices::Pos> vertices(Sphere.Vertices.size());
	for (size_t i = 0; i < Sphere.Vertices.size(); ++i)
	{
		vertices[i].Position = Sphere.Vertices[i].Position;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertices::Pos);

	std::vector<std::uint16_t> indices = Sphere.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mSkyMesh = std::make_unique<MeshGeometry>();
	mSkyMesh->Name = "SkyGeo";

	//CPU Vertex Buffer
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mSkyMesh->VertexBufferCPU));
	CopyMemory(mSkyMesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	//CPU Index Buffer
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mSkyMesh->IndexBufferCPU));
	CopyMemory(mSkyMesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//GPU Vertex Buffer
	mSkyMesh->VertexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		CommandList, vertices.data(), vbByteSize, mSkyMesh->VertexBufferUploader);
	//GPU Index Buffer
	mSkyMesh->IndexBufferGPU = EngineUtility::CreateDefaultBuffer(g_Device,
		CommandList, indices.data(), ibByteSize, mSkyMesh->IndexBufferUploader);

	mSkyMesh->VertexByteStride = sizeof(Vertices::Pos);
	mSkyMesh->VertexBufferByteSize = vbByteSize;
	mSkyMesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	mSkyMesh->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mSkyMesh->DrawArgs["Sky"] = submesh;
}

void CSky::BuildSkySRV(ID3D12GraphicsCommandList* CommandList)
{
	wstring SkyPath;
	Microsoft::WRL::ComPtr<ID3D12Resource> skyTex = nullptr;
	m_Descriptor = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	auto BuildDescriptor = [&]
	{
		skyTex = TextureManager::GetInstance(CommandList)->GetTexture(SkyPath, false)->Resource;
		D3D12_SHADER_RESOURCE_VIEW_DESC SkysrvDesc = {};
		SkysrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SkysrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SkysrvDesc.TextureCube.MostDetailedMip = 0;
		SkysrvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
		SkysrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		SkysrvDesc.Format = skyTex->GetDesc().Format;
		g_Device->CreateShaderResourceView(skyTex.Get(), &SkysrvDesc, m_Descriptor);
	};

	if (CheckFileExist(StringToWString(EngineGlobal::SkyAssetPath)+L"SKY_HDR.DDS"))
	{
		SkyPath = HDR_SKY;
		BuildDescriptor();
	}
	else if(CheckFileExist(StringToWString(EngineGlobal::SkyAssetPath) + L"SKY_LDR.DDS"))
	{
		SkyPath = LDR_SKY;
		BuildDescriptor();
	}
	else
	{
		m_Descriptor = GetDefaultTexture(eBlackCubeMap);
	}
	//set cube sky SRV index
	GlobleSRVIndex::gSkyCubeSRVIndex = AddToGlobalCubeSRVDescriptor(m_Descriptor);
}

void CSky::Draw(GraphicsContext& Context, GraphicsPSO* SpecificPSO)
{
	if (SpecificPSO)
	{
		Context.SetPipelineState(*SpecificPSO);
	}
	else
	{
		Context.SetPipelineState(mPSO);
		Context.SetDynamicConstantBufferView((int)RootSignatureType::eCommonConstantBuffer, sizeof(mWorld), &mWorld);
	}
	Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetIndexBuffer(mSkyMesh->IndexBufferView());
	Context.SetVertexBuffer(0,mSkyMesh->VertexBufferView());
	Context.DrawIndexed(mSkyMesh->DrawArgs["Sky"].IndexCount, mSkyMesh->DrawArgs["Sky"].StartIndexLocation, mSkyMesh->DrawArgs["Sky"].BaseVertexLocation);
}

void CSky::DrawUI()
{
}

void CSky::BuildPSO(const PSOCommonProperty CommonProperty)
{
	D3D12_RASTERIZER_DESC Rasterizer = RasterizerDefault;
	if (CommonProperty.IsWireframe)
	{
		Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
		Rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
	}
	if (CommonProperty.IsMSAA)
	{
		Rasterizer.MultisampleEnable = true;
	}

	mPSO.SetRootSignature(*CommonProperty.pRootSignature);
	mPSO.SetRasterizerState(Rasterizer);
	mPSO.SetBlendState(BlendDisable);
	mPSO.SetDepthStencilState(DepthStateDisabled);
	mPSO.SetSampleMask(0xFFFFFFFF);
	mPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	mPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mPSO.SetVertexShader(g_pSkyVS, sizeof(g_pSkyVS));
	mPSO.SetPixelShader(g_pSkyPS, sizeof(g_pSkyPS));
	mPSO.SetRenderTargetFormat(DefaultHDRColorFormat, g_SceneDepthBuffer.GetFormat(), CommonProperty.MSAACount, CommonProperty.MSAAQuality);
	mPSO.Finalize();
}

void CSky::Update(float dt, GraphicsContext* Context)
{
}
