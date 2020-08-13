#include "ModelLoader.h"


ModelLoader::ModelLoader()
{
}


ModelLoader::~ModelLoader()
{
}


bool ModelLoader::Load(HWND hwnd, ID3D12Device* dev, ID3D12GraphicsCommandList* CommandList, std::string filename)
{
    Assimp::Importer importer;


    const aiScene* pScene = importer.ReadFile(filename,
        aiProcessPreset_TargetRealtime_Quality |
        aiProcess_ConvertToLeftHanded);

    if (pScene == NULL)
    {
        MessageBox(hwnd, L"assimp scene create failed!", L"error", 0);
        return false;
    }

    this->directory = filename.substr(0, filename.find_last_of('/'));



    this->dev = dev;
    this->hwnd = hwnd;
    this->pCommandList = CommandList;
    ProcessNode(pScene->mRootNode, pScene);
    return true;
}



void ModelLoader::Close()
{
}


void ModelLoader::ProcessNode(aiNode* node, const aiScene* scene)
{
    for (UINT i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(this->ProcessMesh(mesh, scene));
    }



    for (UINT i = 0; i < node->mNumChildren; i++)
    {
        this->ProcessNode(node->mChildren[i], scene);
    }
}


string textype;

ModelMesh ModelLoader::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    // Data to fill
    vector<PosNormalTangentTexc> vertices;
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
        PosNormalTangentTexc vertex;

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

    //Material
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        vector<Texture> DiffuseMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);

        //vector<Texture> NormalMaps = this->LoadMaterialTextures(material, aiTextureType_NORMALS, "texture_Normal", scene);
        //vector<Texture> AOMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_ao", scene);
        //vector<Texture> HeightMaps = this->LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_height", scene);
        //vector<Texture> MetallicMaps = this->LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_metallic", scene);
        //vector<Texture> RoughnessMaps = this->LoadMaterialTextures(material, aiTextureType_SHININESS, "texture_roughness", scene);
        //NOW ONLY USE DIFFUSE MAP FOR TEST
        textures.insert(textures.end(), DiffuseMaps.begin(), DiffuseMaps.end());

    }
    return ModelMesh(dev, pCommandList, vertices, indices, textures);
}







string ModelLoader::DetermineTextureType(const aiScene* scene, aiMaterial* mat)
{
    return string();
}



int ModelLoader::GetTextureIndex(aiString* str)
{
    return 0;
}



ID3D12Resource* ModelLoader::GetTextureFromModel(const aiScene* scene, int textureindex)
{
    return nullptr;
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
        for (UINT j = 0; j < textures_loaded.size(); j++)
        {
            if (std::strcmp(textures_loaded[j].Filename.c_str(), str.C_Str()) == 0)
            {
                textures.push_back(textures_loaded[j]);
                skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)
                break;
            }
        }
        //     if (!skip)
        //     {   // If texture hasn't been loaded already, load it
        //            HRESULT hr;
        //            Texture texture;
        //            if (textype == "embedded compressed texture")
        //            {
        //                   int textureindex = GetTextureIndex(&str);
        //                   texture.Resource = GetTextureFromModel(scene, textureindex);
        //            }
        //            else
        //            {
        //                   string filename = string(str.C_Str());
        //                   filename = directory + '/' + filename;
        //                   wstring filenamews = wstring(filename.begin(), filename.end());
        //                   hr = CreateWICTextureFromFile(dev, devcon, filenamews.c_str(), nullptr, &texture.texture);
        //                   if (FAILED(hr))
        //                         MessageBox(hwnd, L"Texture couldn't be loaded", L"Error!", MB_ICONERROR | MB_OK);
        //            }
        //            texture.type = typeName;
        //            texture.path = str.C_Str();
        //            textures.push_back(texture);
        //            this->textures_loaded.push_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
        //     }
    }
    return textures;
}