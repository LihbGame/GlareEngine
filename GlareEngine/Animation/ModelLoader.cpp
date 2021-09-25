#include "ModelLoader.h"
#include "EngineLog.h"
#include "GraphicsCore.h"

using namespace GlareEngine; 
using namespace GlareEngine::DirectX12Graphics;

ModelLoader* ModelLoader::g_ModelLoader = new ModelLoader();

string  PBRTextureFileType[] = {
	"_albedo",
	"_normal",
	"_ao",
	"_metallic",
	"_roughness",
	"_height"
};




ModelLoader::ModelLoader()
{
}

ModelLoader::~ModelLoader()
{
	mImporter.FreeScene();
}

ModelLoader* ModelLoader::GetModelLoader(ID3D12GraphicsCommandList* CommandList)
{
	assert(g_ModelLoader);
	g_ModelLoader->SetCommandList(CommandList);
	return g_ModelLoader;
}

void ModelLoader::Release()
{
	if (g_ModelLoader)
	{
		delete g_ModelLoader;
		g_ModelLoader = nullptr;
	}
}


void ModelLoader::SetCommandList(ID3D12GraphicsCommandList* CommandList)
{
	m_pCommandList = CommandList;
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
	this->mModelName = filename;

	ProcessNode(pScene->mRootNode, pScene);
	return true;
}

const ModelRenderData* ModelLoader::GetModelRenderData(string ModelName)
{
	if (mMeshes.find(ModelName) == mMeshes.end())
	{
		LoadModel(ModelName);
	}
	return &mMeshes[ModelName];
}


void ModelLoader::BuildMaterial(string MaterialName)
{
	MaterialManager::GetMaterialInstance()->BuildMaterials(
		StringToWString(MaterialName),
		LoadPBRTexture(MaterialName),
		0.09f,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT3(0.1f, 0.1f, 0.1f),
		MathHelper::Identity4x4());

	const Material* ModelMat = MaterialManager::GetMaterialInstance()->GetMaterial(StringToWString(MaterialName));
	assert(ModelMat);
	mMeshes[mModelName].mSubModels.back().mMaterial = ModelMat;
}

void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene)
{
	for (UINT i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		mMeshes[mModelName].mSubModels.push_back(ModelData());
		mMeshes[mModelName].mSubModels.back().mMeshData = this->ProcessMesh(mesh, scene);
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
		
		//Build Material
		BuildMaterial(texture);
	}
	return ModelMesh(m_pCommandList, mesh->mName.C_Str(), vertices, indices);
}



vector<Texture*> ModelLoader::LoadPBRTexture(string texturename)
{
	vector<Texture*> ModelTextures;
	string RootFilename = mDirectory + "PBRTextures/" + texturename;
	string Fullfilename = "";

	for (auto Type : PBRTextureFileType)
	{
		Fullfilename = RootFilename + Type;
		ModelTextures.push_back(TextureManager::GetInstance(m_pCommandList)->GetModelTexture(StringToWString(Fullfilename)).get());
	}

	return ModelTextures;
}

