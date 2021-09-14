#include "ModelLoader.h"
#include "EngineLog.h"
#include "GraphicsCore.h"

using namespace GlareEngine; 
using namespace GlareEngine::DirectX12Graphics;


string  PBRTextureFileType[] = {
	"_albedo",
	"_normal",
	"_ao",
	"_metallic",
	"_roughness",
	"_height"
};




ModelLoader::ModelLoader(ID3D12GraphicsCommandList* CommandList)
	:m_pCommandList(CommandList)
{
}

ModelLoader::~ModelLoader()
{
	mImporter.FreeScene();
}


bool ModelLoader::LoadModel(string filename)
{
	mDirectory = "../Resource/Model/";
	string FullName = mDirectory + filename;
	const aiScene* pScene = mImporter.ReadFile(FullName,
		aiProcessPreset_TargetRealtime_Quality |
		aiProcess_ConvertToLeftHanded);

	if (pScene == NULL)
	{
		EngineLog::AddLog(L"Error:Assimp model mesh scene create failed!");
		return false;
	}

	int it = (int)filename.find_last_of('/');
	this->mDirectory += filename.substr(0, (LONG64)(it) + 1);
	this->mModelName = filename.substr((LONG64)(it) + 1, filename.find_last_of('.') - it - 1);

	ProcessNode(pScene->mRootNode, pScene);
	return true;
}


void ModelLoader::CreateSRVDescriptor(vector<Texture*> Texture)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (int i = 0; i < Texture.size(); ++i)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE Descriptor;
		Descriptor =AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		srvDesc.Format = Texture[i]->Resource->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = Texture[i]->Resource->GetDesc().MipLevels;
		g_Device->CreateShaderResourceView(Texture[i]->Resource.Get(), &srvDesc, Descriptor);
		mMeshes[mModelName].mSubMeshes.back().mDescriptors.push_back(Descriptor);
	}
}

void ModelLoader::BuildMaterial(string MaterialName)
{
	XMFLOAT4X4  MatTransform = MathHelper::Identity4x4();

	MaterialManager::GetMaterialInstance()->BuildMaterials(
		wstring(MaterialName.begin(), MaterialName.end()),
		0.09f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MatTransform);
}

void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene)
{
	for (UINT i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		mMeshes[mModelName].mSubMeshes.push_back(SubMesh());
		mMeshes[mModelName].mSubMeshes.back().mMesh = this->ProcessMesh(mesh, scene);
	}

	for (UINT i = 0; i < node->mNumChildren; i++)
	{
		this->ProcessNode(node->mChildren[i], scene);
	}
}


ModelMesh ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	// Data to fill
	vector<Vertices::PosNormalTangentTexc> vertices;
	vector<UINT> indices;

	// Walk through each of the mesh's vertices
	for (UINT i = 0; i < mesh->mNumVertices; i++)
	{
		Vertices::PosNormalTangentTexc vertex;
		//Position
		if (mesh->HasPositions())
		{
			vertex.Position.x = (float)mesh->mVertices[i].x;
			vertex.Position.y = (float)mesh->mVertices[i].y;
			vertex.Position.z = (float)mesh->mVertices[i].z;
		}
		//Normal
		if (mesh->HasNormals())
		{
			vertex.Normal.x = (float)mesh->mNormals[i].x;
			vertex.Normal.y = (float)mesh->mNormals[i].y;
			vertex.Normal.z = (float)mesh->mNormals[i].z;
		}
		//Tangent
		if (mesh->HasTangentsAndBitangents())
		{
			vertex.Tangent.x = (float)mesh->mTangents[i].x;
			vertex.Tangent.y = (float)mesh->mTangents[i].y;
			vertex.Tangent.z = (float)mesh->mTangents[i].z;
		}
		//TextureCoords 0
		if (mesh->HasTextureCoords(0))
		{
			vertex.Texc.x = (float)mesh->mTextureCoords[0][i].x;
			vertex.Texc.y = (float)mesh->mTextureCoords[0][i].y;
		}
		vertices.push_back(vertex);
	}

	for (UINT i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (UINT j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiString texturename;
		material->GetTexture(aiTextureType_DIFFUSE, 0, &texturename);
		string texture(texturename.C_Str());

		int it = (int)texture.find_last_of('\\');
		texture = texture.substr((LONG64)(it) + 1, texture.find_last_of('.') - it - 1);
		LoadPBRTexture(texture);
	}

	return ModelMesh(m_pCommandList, vertices, indices);
}



void ModelLoader::LoadPBRTexture(string texturename)
{
	vector<Texture*> ModelTextures;
	string RootFilename = mDirectory + "PBRTextures/" + texturename;
	string Fullfilename = "";

	for (auto Type:PBRTextureFileType)
	{
		Fullfilename = RootFilename + Type;
		ModelTextures.push_back(TextureManager::GetInstance(m_pCommandList)->GetModelTexture(StringToWString(Fullfilename)).get());
	}

	//Build Material
	BuildMaterial(texturename);
	//Init material const buffer index
	mMeshes[mModelName].mSubMeshes.back().mMaterialIndex = MaterialManager::GetMaterialInstance()->GetMaterial(StringToWString(texturename))->mMatCBIndex;
	//Create SRV Descriptor
	CreateSRVDescriptor(ModelTextures);
}

