#include "ModelLoader.h"
#include "EngineLog.h"

using namespace GlareEngine;

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
	this->mDirectory += filename.substr(0, it + 1);
	this->mModelName = filename.substr(it + 1, filename.find_last_of('.') - it - 1);

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