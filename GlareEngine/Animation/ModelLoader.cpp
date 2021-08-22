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


void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene)
{
	for (UINT i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		mMeshes[mModelName].push_back(this->ProcessMesh(mesh, scene));
	}

	for (UINT i = 0; i < node->mNumChildren; i++)
	{
		this->ProcessNode(node->mChildren[i], scene);
	}
}


ModelMesh ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	// Data to fill
	vector<Vertice::PosNormalTangentTexc> vertices;
	vector<UINT> indices;

	// Walk through each of the mesh's vertices
	for (UINT i = 0; i < mesh->mNumVertices; i++)
	{
		Vertice::PosNormalTangentTexc vertex;
		//Position
		if (mesh->HasPositions())
		{
			vertex.Pos.x = (float)mesh->mVertices[i].x;
			vertex.Pos.y = (float)mesh->mVertices[i].y;
			vertex.Pos.z = (float)mesh->mVertices[i].z;
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
	//Store model's texture name
	mModelTexturesName[mModelName].push_back(texturename);

	string RootFilename = mDirectory + "PBRTextures/" + texturename;
	string Fullfilename = "";
	for (auto Type:PBRTextureFileType)
	{
		Fullfilename = RootFilename + Type;
		mModelTextures[texturename].push_back(g_TextureManager.GetModelTexture(wstring(Fullfilename.begin(), Fullfilename.end())).get());
	}
}
