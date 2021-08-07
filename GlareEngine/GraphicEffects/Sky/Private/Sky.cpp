#include "Sky.h"
#include "GraphicsCore.h"
#include "GeometryGenerator.h"
#include "TextureManager.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "Vertex.h"
#include "InputLayout.h"
//shader
#include "CompiledShaders/SkyVS.h"
#include "CompiledShaders/SkyPS.h"

using namespace GlareEngine::DirectX12Graphics;
CSky::CSky(ID3D12GraphicsCommandList* CommandList,
	float radius, int sliceCount, int stackCount)
{
	BuildSkyMesh(CommandList, radius, sliceCount, stackCount);
	BuildSkySRV();

	//world mat
	XMStoreFloat4x4(&mWorld, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
}

CSky::~CSky()
{
}


void CSky::BuildSkyMesh(ID3D12GraphicsCommandList* CommandList, float radius, int sliceCount, int stackCount)
{
	GeometryGenerator SphereGeo;
	GeometryGenerator::MeshData Sphere = SphereGeo.CreateSphere(radius, sliceCount, stackCount);

	std::vector<Vertice::Pos> vertices(Sphere.Vertices.size());
	for (size_t i = 0; i < Sphere.Vertices.size(); ++i)
	{
		vertices[i].Pos = Sphere.Vertices[i].Position;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertice::Pos);

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

	mSkyMesh->VertexByteStride = sizeof(Vertice::Pos);
	mSkyMesh->VertexBufferByteSize = vbByteSize;
	mSkyMesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	mSkyMesh->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mSkyMesh->DrawArgs["Sky"] = submesh;
}

void CSky::BuildSkySRV()
{
	auto SkyTex = g_TextureManager.GetTexture(L"HDRSky\\Sky",false)->Resource;
	m_Descriptor= AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC SkysrvDesc = {};
	SkysrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SkysrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SkysrvDesc.TextureCube.MostDetailedMip = 0;
	SkysrvDesc.TextureCube.MipLevels = SkyTex->GetDesc().MipLevels;
	SkysrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	SkysrvDesc.Format = SkyTex->GetDesc().Format;
	g_Device->CreateShaderResourceView(SkyTex.Get(), &SkysrvDesc, m_Descriptor);
}

void CSky::Draw(GraphicsContext& Context)
{
	Context.SetPipelineState(mSkyPSO);
	Context.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetDynamicDescriptor(2, 0, m_Descriptor);
	Context.SetIndexBuffer(mSkyMesh->IndexBufferView());
	Context.SetVertexBuffer(0,mSkyMesh->VertexBufferView());
	Context.SetDynamicConstantBufferView(1, sizeof(mWorld), &mWorld);
	Context.DrawIndexed(mSkyMesh->DrawArgs["Sky"].IndexCount, mSkyMesh->DrawArgs["Sky"].StartIndexLocation, mSkyMesh->DrawArgs["Sky"].BaseVertexLocation);
}

void CSky::BuildPSO(const RootSignature& rootSignature)
{

	mSkyPSO.SetRootSignature(rootSignature);
	mSkyPSO.SetRasterizerState(RasterizerDefault);
	mSkyPSO.SetBlendState(BlendDisable);
	mSkyPSO.SetDepthStencilState(DepthStateDisabled);
	mSkyPSO.SetSampleMask(0xFFFFFFFF);
	mSkyPSO.SetInputLayout((UINT)InputLayout::Pos.size(), InputLayout::Pos.data());
	mSkyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	mSkyPSO.SetVertexShader(g_pSkyVS, sizeof(g_pSkyVS));
	mSkyPSO.SetPixelShader(g_pSkyPS, sizeof(g_pSkyPS));
	mSkyPSO.SetRenderTargetFormat(DefaultHDRColorFormat, DXGI_FORMAT_UNKNOWN);
	mSkyPSO.Finalize();

}
