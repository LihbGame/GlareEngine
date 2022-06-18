#include "./ModelLoader.h"
#include "Material.h"

ModelLoader::ModelLoader(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList,TextureManage* TextureManage)
:dev(dev),
hwnd(hwnd),
pCommandList(CommandList),
pTextureManage(TextureManage)
{
}


ModelLoader::~ModelLoader()
{
    importer.FreeScene();
}


bool ModelLoader::LoadModel(string filename)
{
    directory = "../Resource/Model/";

    string FullName= directory + filename;
    const aiScene* pScene = importer.ReadFile(FullName,
        aiProcessPreset_TargetRealtime_Quality |
        aiProcess_ConvertToLeftHanded);

    if (pScene == NULL)
    {
        MessageBox(hwnd, L"assimp model mesh scene create failed!", L"error", 0);
        return false;
    }

    

    int it = (int)filename.find_last_of('/');
    this->directory += filename.substr(0, it +1);

    this->ModelName = filename.substr(it + 1, filename.find_last_of('.') - it - 1);
    ProcessNode(pScene->mRootNode, pScene,false);
    return true;
}

bool ModelLoader::LoadAnimation(string filename)
{
    directory = "Model/";


    string FullName = directory + filename;
    const aiScene* pAnimeScene = importer.ReadFile(FullName,
        aiProcessPreset_TargetRealtime_Quality);
    if (pAnimeScene == NULL)
    {
        MessageBox(hwnd, L"assimp animation scene create failed!", L"error", 0);
        return false;
    }

  
    int it = (int)filename.find_last_of('@');
    //this->directory += filename.substr(0, it + 1);
    this->ModelName= filename.substr(filename.find_last_of('/') + 1, filename.find_last_of('@') - filename.find_last_of('/') - 1);
    this->AnimeName = filename.substr(it + 1, filename.find_last_of('.') - it - 1);
   
    
    m_global_inverse_transform = pAnimeScene->mRootNode->mTransformation;
    m_global_inverse_transform.Inverse();
    
    mAnimations[ModelName][AnimeName].m_global_inverse_transform = m_global_inverse_transform;
    mAnimations[ModelName][AnimeName].pAnimeScene = pAnimeScene;


    if (pAnimeScene->mAnimations[0]->mTicksPerSecond != 0.0)
    {
        mAnimations[ModelName][AnimeName].ticks_per_second = (float)pAnimeScene->mAnimations[0]->mTicksPerSecond;
    }
    else
    {
        mAnimations[ModelName][AnimeName].ticks_per_second = 25.0f;
    }


    ProcessNode(pAnimeScene->mRootNode, pAnimeScene,true);
    //init anime
   // mAnimations[ModelName][AnimeName].InitAnime();

    //mAnimations[ModelName][AnimeName].SetUpMesh(dev, pCommandList);
    return true;
}

void ModelLoader::DrawModel(string ModelName)
{
}

vector<::ModelMesh>& ModelLoader::GetModelMesh(string ModelName)
{
    return meshes[ModelName];
}

unordered_map<string, vector<Texture*>>& ModelLoader::GetAllModelTextures()
{
    return ModelTextures;
}



vector<string> ModelLoader::GetModelTextureNames(string modelname)
{
    return ModelTexturesName[modelname];
}

void ModelLoader::Close()
{
}

void ModelLoader::BuildMaterials()
{
    XMFLOAT4X4  MatTransform = MathHelper::Identity4x4();
	for (auto e : ModelTextures)
	{
        Materials::GetMaterialInstance()->BuildMaterials(
			wstring(e.first.begin(), e.first.end()),
			0.09f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3(0.1f, 0.1f, 0.1f),
			MatTransform,
			MaterialType::ModelPBRMat);
	}
}

void ModelLoader::FillSRVDescriptorHeap(int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor)
{
	vector<ID3D12Resource*> PBRTexResource;
	PBRTexResource.resize(PBRTextureType::Count);
	for (auto& e : ModelTextures)
	{
		PBRTexResource[PBRTextureType::DiffuseSrvHeapIndex] = e.second[0]->Resource.Get();
		PBRTexResource[PBRTextureType::NormalSrvHeapIndex] = e.second[1]->Resource.Get();
		PBRTexResource[PBRTextureType::AoSrvHeapIndex] = e.second[2]->Resource.Get();
		PBRTexResource[PBRTextureType::MetallicSrvHeapIndex] = e.second[3]->Resource.Get();
		PBRTexResource[PBRTextureType::RoughnessSrvHeapIndex] = e.second[4]->Resource.Get();
		PBRTexResource[PBRTextureType::HeightSrvHeapIndex] = e.second[5]->Resource.Get();

		pTextureManage->CreatePBRSRVinDescriptorHeap(PBRTexResource, SRVIndex, hDescriptor, wstring(e.first.begin(), e.first.end()));
	}
}


void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene,bool isAnimation)
{
    for (UINT i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        if (!isAnimation)
        {
            meshes[ModelName].push_back(this->ProcessMesh(mesh, scene));
        }
        else
        {
            mAnimations[ModelName][AnimeName].mBoneMeshs.push_back(this->ProcessAnimation(mesh, scene));
        }
    }

    for (UINT i = 0; i < node->mNumChildren; i++)
    {
        this->ProcessNode(node->mChildren[i], scene, isAnimation);
    }
}


string textype;

::ModelMesh ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    // Data to fill
    vector<Vertices::PosNormalTangentTexc> vertices;
    vector<UINT> indices;
    vector<Texture> textures;

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        if (textype.empty())
            textype = DetermineTextureType(scene, mat);
    }



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

        if (mesh->mTextureCoords[0])
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

    ////Material
    //if (mesh->mMaterialIndex >= 0)
    //{
    //    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    //    vector<Texture> DiffuseMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
    //    //vector<Texture> NormalMaps = this->LoadMaterialTextures(material, aiTextureType_NORMALS, "texture_Normal", scene);
    //    //vector<Texture> AOMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_ao", scene);
    //    //vector<Texture> HeightMaps = this->LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_height", scene);
    //    //vector<Texture> MetallicMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_metallic", scene);
    //    //vector<Texture> RoughnessMaps = this->LoadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness", scene);
    //    //NOW ONLY USE DIFFUSE MAP FOR TEST
    //    textures.insert(textures.end(), DiffuseMaps.begin(), DiffuseMaps.end());
    //}

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texturename;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texturename);
        string texture(texturename.C_Str());

        int it = (int)texture.find_last_of('\\');
        texture = texture.substr(it + 1, texture.find_last_of('.') - it - 1);
        LoadPBRTexture(texture);
    }


    return ::ModelMesh(dev, pCommandList, vertices, indices, textures);
}

AnimationMesh ModelLoader::ProcessAnimation(aiMesh* mesh, const aiScene* scene)
{
    AnimationMesh LAnime;
    LAnime.bones_id_weights_for_each_vertex.resize(mesh->mNumVertices);
    // load bones
    for (UINT i = 0; i < mesh->mNumBones; i++)
    {
        UINT bone_index = 0;
        string bone_name(mesh->mBones[i]->mName.data);
#if defined(_DEBUG)
        string DebugInfo(ModelName + "-->" + AnimeName + ":" + bone_name+"\n");
        ::OutputDebugStringA(DebugInfo.c_str());
#endif

        //if (mAnimations[ModelName][AnimeName].m_bone_mapping.find(bone_name)
        //    == mAnimations[ModelName][AnimeName].m_bone_mapping.end())
        //{
        //    // Allocate an index for a new bone
        //    bone_index = mAnimations[ModelName][AnimeName].m_num_bones++;
        //    BoneMatrix bi;
        //    mAnimations[ModelName][AnimeName].m_bone_matrices.push_back(bi);
        //    mAnimations[ModelName][AnimeName].m_bone_matrices[bone_index].Offset_Matrix = mesh->mBones[i]->mOffsetMatrix;
        //    mAnimations[ModelName][AnimeName].m_bone_mapping[bone_name] = bone_index;  
        //}
        //else
        //{
            bone_index = mBoneNames[bone_name];// mAnimations[ModelName][AnimeName].m_bone_mapping[bone_name];
        //}
        
        for (UINT j = 0; j < mesh->mBones[i]->mNumWeights; j++)
        {
            UINT vertex_id = mesh->mBones[i]->mWeights[j].mVertexId; 
            float weight = mesh->mBones[i]->mWeights[j].mWeight;
            LAnime.bones_id_weights_for_each_vertex[vertex_id].addBoneData(bone_index, weight);
        }
    }


    LAnime.SetUpMesh(dev, pCommandList);//bone data for IA stage

    return LAnime;
}



string ModelLoader::DetermineTextureType(const aiScene* scene, aiMaterial* mat)
{
    aiString textypeStr;
    mat->GetTexture(aiTextureType_DIFFUSE, 0, &textypeStr);
    string textypeteststr = textypeStr.C_Str();
    if (textypeteststr == "*0" || textypeteststr == "*1" || textypeteststr == "*2" || textypeteststr == "*3" || textypeteststr == "*4" || textypeteststr == "*5")
    {
        if (scene->mTextures[0]->mHeight == 0)
        {
            return "embedded compressed texture";
        }
        else
        {
            return "embedded non-compressed texture";
        }
    }
    if (textypeteststr.find('.') != string::npos)
    {
        return "textures are on disk";
    }
    return " ";
}



int ModelLoader::GetTextureIndex(aiString* str)
{
    string tistr;
    tistr = str->C_Str();
    tistr = tistr.substr(1);
    return stoi(tistr);
}



void ModelLoader::GetTextureFromModel(const aiScene* scene, int textureindex, Texture& texture)
{
    int* size = reinterpret_cast<int*>(&scene->mTextures[textureindex]->mWidth);
    EngineUtility::CreateWICTextureFromMemory(dev, pCommandList, texture.Resource.ReleaseAndGetAddressOf(), texture.UploadHeap.ReleaseAndGetAddressOf(), reinterpret_cast<unsigned char*>(scene->mTextures[textureindex]->pcData),*size);
}

void ModelLoader::LoadPBRTexture(string texturename)
{
    //store model's texture name
    ModelTexturesName[ModelName].push_back(texturename);

    string rootfilename = directory +"PBRTextures/"+ texturename;
    //aldedo
    string Fullfilenames = rootfilename + "_albedo";
    ModelTextures[texturename].push_back(pTextureManage->GetModelTexture(wstring(Fullfilenames.begin(), Fullfilenames.end())).get());
    //ao
    Fullfilenames = rootfilename + "_normal";
    ModelTextures[texturename].push_back(pTextureManage->GetModelTexture(wstring(Fullfilenames.begin(), Fullfilenames.end())).get());
    //Metallic
    Fullfilenames = rootfilename + "_ao";
    ModelTextures[texturename].push_back(pTextureManage->GetModelTexture(wstring(Fullfilenames.begin(), Fullfilenames.end())).get());
    //normal
    Fullfilenames = rootfilename + "_metallic";
    ModelTextures[texturename].push_back(pTextureManage->GetModelTexture(wstring(Fullfilenames.begin(), Fullfilenames.end())).get());
    //Roughness
    Fullfilenames = rootfilename + "_roughness";
    ModelTextures[texturename].push_back(pTextureManage->GetModelTexture(wstring(Fullfilenames.begin(), Fullfilenames.end())).get());
    //Height
    Fullfilenames = rootfilename + "_height";
    ModelTextures[texturename].push_back(pTextureManage->GetModelTexture(wstring(Fullfilenames.begin(), Fullfilenames.end())).get());
}



vector<Texture> ModelLoader::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene)
{
    vector<Texture> textures;
    for (UINT i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        // Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
        bool skip = false;
        for (UINT j = 0; j < Textures.size(); j++)
        {
            if (std::strcmp(Textures[j].Filename.c_str(), str.C_Str()) == 0)
            {
                textures.push_back(Textures[j]);
                skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)
                break;
            }
        }
        if (!skip)
        {   // If texture hasn't been loaded already, load it
            Texture texture;
            if (textype == "embedded compressed texture")
            {
                int textureindex = GetTextureIndex(&str);
                GetTextureFromModel(scene, textureindex,texture);
            }
            else
            {
                string filename = string(str.C_Str());
                int it = (int)filename.find_last_of('\\');
                filename = filename.substr(it+1, filename.size()-it);

                filename = directory + filename;
                wstring filenamews = wstring(filename.begin(), filename.end());
                EngineUtility::CreateWICTextureFromFile(dev, pCommandList, texture.Resource.ReleaseAndGetAddressOf(), texture.UploadHeap.ReleaseAndGetAddressOf(), filenamews);
            }
            texture.type = typeName;
            texture.Filename = str.C_Str();
            textures.push_back(texture);
            this->Textures.push_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
        }
    }
    return textures;
}