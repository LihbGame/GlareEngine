#include "SimpleModelGenerator.h"
#include "Graphics/TextureManager.h"

const char* ModelPBRMaterials[]
{
	"PBRBrass",
	"PBRGrass",
	"PBRGrass01",
	"PBRharshbricks",
	"PBRIndustrial_narrow_brick",
	"PBRrocky_shoreline1",
	"PBRstylized_grass1",
	"PBRwhite_spruce_tree_bark",
	"PBRRock046S"
};


SimpleModelGenerator* SimpleModelGenerator::S_SimpleModelGenerator = new SimpleModelGenerator;
ID3D12GraphicsCommandList* SimpleModelGenerator::pCommandList = nullptr;

void SimpleModelGenerator::CreatePBRMaterials()
{
	int MaterialNum = sizeof(ModelPBRMaterials) / sizeof(char*);
	for (int i = 0; i < MaterialNum; ++i)
	{
		vector<Texture*> ModelTextures;
		string Filename = EngineGlobal::TextureAssetPath;
		Filename += ModelPBRMaterials[i]+ string("/") + ModelPBRMaterials[i];
		TextureManager::GetInstance(pCommandList)->CreatePBRTextures(Filename, ModelTextures);
		MaterialManager::GetMaterialInstance()->BuildMaterials(
			StringToWString(ModelPBRMaterials[i]),
			ModelTextures,
			0.04f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT3(0.1f, 0.1f, 0.1f),
			MathHelper::Identity4x4());
	}
}

ModelRenderData* SimpleModelGenerator::CreateSimpleModelRanderData(string ModelName, SimpleModelType Type, string MaterialName)
{
	if (mMeshes.find(ModelName) == mMeshes.end())
	{
		ModelRenderData lModelRenderData;
		lModelRenderData.mSubModels.resize(1);
		ModelData& lModelData = lModelRenderData.mSubModels[0];
		MeshData lMeshData;
		char* MeshName = (char*)"";
		switch (Type)
		{
		case SimpleModelType::Box:
		{
			lMeshData = mGeometryGenerator.CreateBox(100.0f, 100.0f, 100.0f, 1);
			MeshName = (char*)"Box";
			break;
		}
		case SimpleModelType::Cylinder:
		{
			lMeshData = mGeometryGenerator.CreateCylinder(20.0f, 5.0f, 50.0f, 30, 30);
			MeshName = (char*)"Cylinder";
			break;
		}
		case SimpleModelType::Grid:
		{
			lMeshData = mGeometryGenerator.CreateGrid(100.0f, 100.0f, 10, 10);
			MeshName = (char*)"Grid";
			break;
		}
		case SimpleModelType::Sphere:
		{
			lMeshData = mGeometryGenerator.CreateSphere(20.0f, 30, 30);
			MeshName = (char*)"Sphere";
			break;
		}
		case SimpleModelType::ScreenQuad:
		{
			lMeshData = mGeometryGenerator.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
			MeshName = (char*)"ScreenQuad";
			break;
		}
		default:
			break;
		}
		lModelData.mMeshData = ModelMesh(pCommandList, MeshName, lMeshData.Vertices, lMeshData.Indices32);
		lModelData.mMaterial = MaterialManager::GetMaterialInstance()->GetMaterial(StringToWString(MaterialName));

		mMeshes[ModelName] = lModelRenderData;
	}
	return &mMeshes[ModelName];
}
