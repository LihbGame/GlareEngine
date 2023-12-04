#include "glTFModelLoader.h"
#include "Engine/EngineLog.h"
#include "Model.h"
#include "glTF.h"
#include "Graphics/TextureManager.h"
#include "Graphics/TextureConvert.h"
#include "Graphics/GraphicsCommon.h"
#include "Graphics/UploadBuffer.h"
#include "MeshBuilder.h"
#include "Graphics/SamplerManager.h"
#include "Graphics/Render.h"
#include "InstanceModel/glTFInstanceModel.h"

//Sampler Permutations
std::unordered_map<uint32_t, uint32_t> gSamplerPermutations;

D3D12_CPU_DESCRIPTOR_HANDLE GetSampler(uint32_t addressModes)
{
	SamplerDesc samplerDesc;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE(addressModes & 0x3);
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE(addressModes >> 2);
	return samplerDesc.CreateDescriptor();
}

void GlareEngine::CompileMesh(
	std::vector<Mesh*>& meshList,
	std::vector<byte>& bufferMemory,
	glTF::Mesh& srcMesh,
	uint32_t matrixIdx,
	const Matrix4& localToObject,
	Math::BoundingSphere& boundingSphere,
	Math::AxisAlignedBox& boundingBox)
{
	size_t totalVertexSize = 0;
	size_t totalDepthVertexSize = 0;
	size_t totalIndexSize = 0;

	Math::BoundingSphere sphereOS(eZero);
	Math::AxisAlignedBox bboxOS(eZero);

	std::vector<Primitive> primitives(srcMesh.primitives.size());
	for (uint32_t i = 0; i < primitives.size(); ++i)
	{
		BuildMesh(primitives[i], srcMesh.primitives[i], localToObject);
		sphereOS = sphereOS.Union(primitives[i].m_BoundsOS);
		bboxOS.AddBoundingBox(primitives[i].m_BBoxOS);
	}

	boundingSphere = sphereOS;
	boundingBox = bboxOS;

	std::map<uint32_t, std::vector<Primitive*>> renderMeshes;
	for (auto& prim : primitives)
	{
		uint32_t hash = prim.hash;
		renderMeshes[hash].push_back(&prim);
		
		//count all primitive vertex and index
		totalVertexSize += prim.VB->size();
		totalDepthVertexSize += prim.DepthVB->size();
		totalIndexSize += Math::AlignUp(prim.IB->size(), 4);
	}

	uint32_t totalBufferSize = (uint32_t)(totalVertexSize + totalDepthVertexSize + totalIndexSize);

	ByteArray stagingBuffer;
	stagingBuffer.reset(new std::vector<byte>(totalBufferSize));
	uint8_t* uploadMem = stagingBuffer->data();

	uint32_t curVBOffset = 0;
	uint32_t curDepthVBOffset = (uint32_t)totalVertexSize;
	uint32_t curIBOffset = curDepthVBOffset + (uint32_t)totalDepthVertexSize;

	for (auto& iter : renderMeshes)
	{
		size_t numDraws = iter.second.size();
		Mesh* mesh = (Mesh*)malloc(sizeof(Mesh) + sizeof(Mesh::Draw) * (numDraws - 1));
		size_t vbSize = 0;
		size_t vbDepthSize = 0;
		size_t ibSize = 0;

		// Compute local space bounding sphere for all subMeshes
		Math::BoundingSphere collectiveSphere(eZero);

		for (auto& draw : iter.second)
		{
			vbSize += draw->VB->size();
			vbDepthSize += draw->DepthVB->size();
			ibSize += draw->IB->size();
			collectiveSphere = collectiveSphere.Union(draw->m_BoundsLS);
		}

		mesh->bounds[0] = collectiveSphere.GetCenter().GetX();
		mesh->bounds[1] = collectiveSphere.GetCenter().GetY();
		mesh->bounds[2] = collectiveSphere.GetCenter().GetZ();
		mesh->bounds[3] = collectiveSphere.GetRadius();
		mesh->vbOffset = (uint32_t)bufferMemory.size() + curVBOffset;
		mesh->vbSize = (uint32_t)vbSize;
		mesh->vbDepthOffset = (uint32_t)bufferMemory.size() + curDepthVBOffset;
		mesh->vbDepthSize = (uint32_t)vbDepthSize;
		mesh->ibOffset = (uint32_t)bufferMemory.size() + curIBOffset;
		mesh->ibSize = (uint32_t)ibSize;
		mesh->vbStride = (uint8_t)iter.second[0]->vertexStride;
		mesh->ibFormat = uint8_t(iter.second[0]->index32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT);
		mesh->meshCBV = (uint16_t)matrixIdx;
		mesh->materialCBV = iter.second[0]->materialIdx;
		mesh->psoFlags = iter.second[0]->psoFlags;
		mesh->pso = 0xFFFF;
		if (srcMesh.skin >= 0)
		{
			mesh->numJoints = 0xFFFF;
			mesh->startJoint = (uint16_t)srcMesh.skin;
		}
		else
		{
			mesh->numJoints = 0;
			mesh->startJoint = 0xFFFF;
		}

		mesh->numDraws = (uint16_t)numDraws;

		uint32_t drawIdx = 0;
		uint32_t curVertOffset = 0;
		uint32_t curIndexOffset = 0;
		for (auto& draw : iter.second)
		{
			Mesh::Draw& d = mesh->draw[drawIdx++];
			d.primCount = draw->primCount;
			d.baseVertex = curVertOffset;
			d.startIndex = curIndexOffset;

			std::memcpy(uploadMem + curVBOffset + curVertOffset, draw->VB->data(), draw->VB->size());
			curVertOffset += (uint32_t)draw->VB->size() / draw->vertexStride;

			std::memcpy(uploadMem + curDepthVBOffset, draw->DepthVB->data(), draw->DepthVB->size());
			std::memcpy(uploadMem + curIBOffset + curIndexOffset, draw->IB->data(), draw->IB->size());
			curIndexOffset += (uint32_t)draw->IB->size() >> (draw->index32 + 1);
		}

		curVBOffset += (uint32_t)vbSize;
		curDepthVBOffset += (uint32_t)vbDepthSize;
		curIBOffset += (uint32_t)Math::AlignUp(ibSize, 4);
		curIndexOffset = Math::AlignUp(curIndexOffset, 4);

		meshList.push_back(mesh);
	}

	bufferMemory.insert(bufferMemory.end(), stagingBuffer->begin(), stagingBuffer->end());
}


static uint32_t WalkGraph(
	std::vector<GraphNode>& sceneGraph,
	Math::BoundingSphere& modelBSphere,
	AxisAlignedBox& modelBBox,
	std::vector<Mesh*>& meshList,
	std::vector<byte>& bufferMemory,
	const std::vector<glTF::Node*>& siblings,
	uint32_t currentPos,
	const Matrix4& transform)
{
	size_t numSiblings = siblings.size();

	for (size_t i = 0; i < numSiblings; ++i)
	{
		glTF::Node* curNode = siblings[i];
		GraphNode& thisGraphNode = sceneGraph[currentPos];
		thisGraphNode.hasChildren = 0;
		thisGraphNode.hasSibling = 0;
		thisGraphNode.matrixIdx = currentPos;
		thisGraphNode.skeletonRoot = curNode->skeletonRoot;
		curNode->linearIdx = currentPos;

		// They might not be used, but we have space to hold the neutral values which could be
		// useful when updating the matrix via animation.
		std::memcpy((float*)&thisGraphNode.scale, curNode->scale, sizeof(curNode->scale));
		std::memcpy((float*)&thisGraphNode.rotation, curNode->rotation, sizeof(curNode->rotation));

		if (curNode->hasMatrix)
		{
			std::memcpy((float*)&thisGraphNode.transform, curNode->matrix, sizeof(curNode->matrix));
		}
		else
		{
			thisGraphNode.transform = Matrix4(
				Matrix3(thisGraphNode.rotation) * Matrix3::MakeScale(thisGraphNode.scale),
				Vector3(*(const XMFLOAT3*)curNode->translation)
			);
		}

		const Matrix4 LocalTransform = thisGraphNode.transform * transform;

		if (!curNode->pointsToCamera && curNode->mesh != nullptr)
		{
			Math::BoundingSphere sphereOS;
			AxisAlignedBox boxOS;
			CompileMesh(meshList, bufferMemory, *curNode->mesh, currentPos, LocalTransform, sphereOS, boxOS);
			modelBSphere = modelBSphere.Union(sphereOS);
			modelBBox.AddBoundingBox(boxOS);
		}

		uint32_t nextPos = currentPos + 1;

		if (curNode->children.size() > 0)
		{
			thisGraphNode.hasChildren = 1;
			nextPos = WalkGraph(sceneGraph, modelBSphere, modelBBox, meshList, bufferMemory, curNode->children, nextPos, LocalTransform);
		}

		// Are there more siblings?
		if (i + 1 < numSiblings)
		{
			thisGraphNode.hasSibling = 1;
		}

		currentPos = nextPos;
	}

	return currentPos;
}



inline void SetTextureOptions(std::map<std::string, uint8_t>& optionsMap, glTF::Texture* texture, uint8_t options)
{
	if (texture && texture->source && optionsMap.find(texture->source->path) == optionsMap.end())
		optionsMap[texture->source->path] = options;
}

void BuildMaterials(glTFModelData& model, const glTF::Asset& asset)
{
	//CBVs need 256 byte alignment
	assert((_alignof(MaterialConstants) & 255) == 0);

	//Replace texture filename extensions with "DDS" in the string table
	model.m_TextureNames.resize(asset.m_images.size());
	for (size_t i = 0; i < asset.m_images.size(); ++i)
		model.m_TextureNames[i] = asset.m_images[i].path;

	std::map<std::string, uint8_t> textureOptions;

	const uint32_t numMaterials = (uint32_t)asset.m_materials.size();

	model.m_MaterialConstants.resize(numMaterials);
	model.m_MaterialTextures.resize(numMaterials);

	for (uint32_t i = 0; i < numMaterials; ++i)
	{
		const glTF::Material& srcMat = asset.m_materials[i];

		MaterialConstantData& material = model.m_MaterialConstants[i];
		material.baseColorFactor[0] = srcMat.baseColorFactor[0];
		material.baseColorFactor[1] = srcMat.baseColorFactor[1];
		material.baseColorFactor[2] = srcMat.baseColorFactor[2];
		material.baseColorFactor[3] = srcMat.baseColorFactor[3];

		material.emissiveFactor[0] = srcMat.emissiveFactor[0];
		material.emissiveFactor[1] = srcMat.emissiveFactor[1];
		material.emissiveFactor[2] = srcMat.emissiveFactor[2];

		material.normalTextureScale = srcMat.normalTextureScale;
		material.metallicFactor = srcMat.metallicFactor;
		material.roughnessFactor = srcMat.roughnessFactor;
		material.clearCoatFactor = srcMat.clearCoatFactor;
		material.flags = srcMat.flags;
		material.specialFlags = srcMat.specialFlags;
		//Default ID=0
		material.shaderModelID = 1;

		MaterialTextureData& dstMat = model.m_MaterialTextures[i];
		dstMat.addressModes = 0;

		for (uint32_t ti = 0; ti < eNumTextures; ++ti)
		{
			dstMat.stringIdx[ti] = 0xFFFF;

			if (srcMat.textures[ti] != nullptr)
			{
				if (srcMat.textures[ti]->source != nullptr)
				{
					dstMat.stringIdx[ti] = uint16_t(srcMat.textures[ti]->source - asset.m_images.data());
				}

				if (srcMat.textures[ti]->sampler != nullptr)
				{
					dstMat.addressModes |= srcMat.textures[ti]->sampler->wrapS << (ti * 4);
					dstMat.addressModes |= srcMat.textures[ti]->sampler->wrapT << (ti * 4 + 2);
				}
				else
				{
					dstMat.addressModes |= 0x5 << (ti * 4);
				}
			}
			else
			{
				dstMat.addressModes |= 0x5 << (ti * 4);
			}
		}

		SetTextureOptions(textureOptions, srcMat.textures[eBaseColor], TextureOptions(true, srcMat.alphaBlend | srcMat.alphaTest));
		SetTextureOptions(textureOptions, srcMat.textures[eMetallicRoughness], TextureOptions(false));
		SetTextureOptions(textureOptions, srcMat.textures[eOcclusion], TextureOptions(false));
		SetTextureOptions(textureOptions, srcMat.textures[eEmissive], TextureOptions(true));
		SetTextureOptions(textureOptions, srcMat.textures[eNormal], TextureOptions(false));
		SetTextureOptions(textureOptions, srcMat.textures[eClearCoat], TextureOptions(false));
	}

	model.m_TextureOptions.clear();
	for (auto name : model.m_TextureNames)
	{
		auto iter = textureOptions.find(name);
		if (iter != textureOptions.end())
		{
			model.m_TextureOptions.push_back(iter->second);
			CompileDDSTexture(asset.m_basePath + StringToWString(iter->first), iter->second);
		}
		else
		{
			model.m_TextureOptions.push_back(0xFF);
		}
	}
	assert(model.m_TextureOptions.size() == model.m_TextureNames.size());
}


void LoadMaterials(Model& model,
	const std::vector<MaterialTextureData>& materialTextures,
	const std::vector<std::wstring>& textureNames,
	const std::vector<uint8_t>& textureOptions,
	const std::wstring& basePath)
{
	//CBVs need 256 byte alignment
	assert((_alignof(MaterialConstants) & 255) == 0);

	// Load textures
	const uint32_t numTextures = (uint32_t)textureNames.size();
	model.m_Textures.resize(numTextures);
	for (size_t ti = 0; ti < numTextures; ++ti)
	{
		std::wstring originalFile = basePath + textureNames[ti];
		CompileDDSTexture(originalFile, textureOptions[ti]);

		std::wstring ddsFile = RemoveExtension(originalFile) + L".dds";

		GraphicsContext& InitializeContext = GraphicsContext::Begin(L"Load Models Textures");
		ID3D12GraphicsCommandList* CommandList = InitializeContext.GetCommandList();

		bool bSRGB = (textureOptions[ti] & eSRGB) != 0;
		model.m_Textures[ti] = TextureManager::GetInstance(CommandList)->GetModelTexture(ddsFile, bSRGB ? true : false);
		InitializeContext.Finish();
	}

	// Generate descriptor tables and record offsets for each material
	const uint32_t numMaterials = (uint32_t)materialTextures.size();
	std::vector<uint32_t> tableOffsets(numMaterials);

	for (uint32_t matIdx = 0; matIdx < numMaterials; ++matIdx)
	{
		const MaterialTextureData& srcMat = materialTextures[matIdx];

		uint32_t DestCount = eNumTextures;
		uint32_t SourceCounts[eNumTextures] = { 1, 1, 1, 1, 1,1 };

		D3D12_CPU_DESCRIPTOR_HANDLE DefaultTextures[eNumTextures] =
		{
			GetDefaultTexture(eBlackOpaque2D),
			GetDefaultTexture(eWhiteOpaque2D),
			GetDefaultTexture(eWhiteOpaque2D),
			GetDefaultTexture(eBlackTransparent2D),
			GetDefaultTexture(eDefaultNormalMap),
			GetDefaultTexture(eBlackOpaque2D),
		};

		D3D12_CPU_DESCRIPTOR_HANDLE SourceTextures[eNumTextures];
		for (uint32_t j = 0; j < eNumTextures; ++j)
		{
			if (srcMat.stringIdx[j] == 0xffff /*|| model.m_Textures[srcMat.stringIdx[j]] == nullptr*/)
				SourceTextures[j] = DefaultTextures[j];
			else
				SourceTextures[j] = model.m_Textures[srcMat.stringIdx[j]]->GetSRV();

			AddToGlobalTextureSRVDescriptor(SourceTextures[j]);
		}

		uint32_t SRVDescriptorTable = MAXCUBESRVSIZE + COMMONSRVSIZE + COMMONUAVSIZE + g_TextureSRV.size() - eNumTextures;

		// See if this combination of samplers has been used before. 
		// If not, allocate more from the heap and copy in the descriptors.
		uint32_t addressModes = srcMat.addressModes;
		auto samplerMapLookup = gSamplerPermutations.find(addressModes);

		if (samplerMapLookup == gSamplerPermutations.end())
		{
			DescriptorHandle SamplerHandles = Render::gSamplerHeap.Alloc(eNumTextures);
			uint32_t SamplerDescriptorTable = Render::gSamplerHeap.GetOffsetOfHandle(SamplerHandles);
			gSamplerPermutations[addressModes] = SamplerDescriptorTable;
			//Recode Descriptor Table offsets
			tableOffsets[matIdx] = SRVDescriptorTable | SamplerDescriptorTable << 16;

			D3D12_CPU_DESCRIPTOR_HANDLE SourceSamplers[eNumTextures];
			for (uint32_t j = 0; j < eNumTextures; ++j)
			{
				SourceSamplers[j] = GetSampler(addressModes & 0xF);
				addressModes >>= 4;
			}
			g_Device->CopyDescriptors(1, &SamplerHandles, &DestCount,
				DestCount, SourceSamplers, SourceCounts, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
		else
		{
			tableOffsets[matIdx] = SRVDescriptorTable | samplerMapLookup->second << 16;
		}
	}

	// Update table offsets for each mesh
	uint8_t* meshPtr = model.m_MeshData.get();
	for (uint32_t i = 0; i < model.m_NumMeshes; ++i)
	{
		Mesh& mesh = *(Mesh*)meshPtr;
		uint32_t offsetPair = tableOffsets[mesh.materialCBV];
		//SRV Table Offset
		mesh.srvTable = offsetPair & 0xFFFF;
		//Sampler table Offset
		mesh.samplerTable = offsetPair >> 16;
		//Get PSO
		mesh.pso = glTFInstanceModel::GetPSO(mesh.psoFlags);
		//offset to next mesh
		meshPtr += sizeof(Mesh) + (mesh.numDraws - 1) * sizeof(Mesh::Draw);
	}
}


void BuildAnimations(glTFModelData& model, const glTF::Asset& asset)
{
	size_t numAnimations = asset.m_animations.size();
	if (numAnimations == 0)
		return;

	model.m_Animations.resize(numAnimations);
	uint32_t animIdx = 0;

	for (const glTF::Animation& anim : asset.m_animations)
	{
		AnimationSet& animSet = model.m_Animations[animIdx++];
		animSet.duration = 0.0f;
		animSet.firstCurve = (uint32_t)model.m_AnimationCurves.size();
		animSet.numCurves = (uint32_t)anim.m_channels.size();

		for (size_t i = 0; i < animSet.numCurves; ++i)
		{
			const glTF::AnimChannel& channel = anim.m_channels[i];
			const glTF::AnimSampler& sampler = *channel.m_sampler;

			assert(channel.m_target->linearIdx >= 0);

			AnimationCurve curve;
			curve.TargetNode = channel.m_target->linearIdx;
			curve.TargetTransform = channel.m_path;
			curve.Interpolation = sampler.m_interpolation;
			curve.KeyFrameOffset = model.m_AnimationKeyFrameData.size();
			curve.KeyFrameFormat = std::min<uint32_t>(sampler.m_output->componentType, AnimationCurve::eFloat);
			curve.NumSegments = sampler.m_output->count - 1.0f;

			// In glTF, stride==0 means "packed tightly"
			if (sampler.m_output->stride == 0)
			{
				uint32_t numComponents = sampler.m_output->type + 1;
				uint32_t bytesPerComponent = sampler.m_output->componentType / 2 + 1;
				curve.KeyFrameStride = numComponents * bytesPerComponent / 4;
			}
			else
			{
				assert(sampler.m_output->stride <= 16 && sampler.m_output->stride % 4 == 0);
				curve.KeyFrameStride = sampler.m_output->stride / 4;
			}

			// Determine start and stop time stamps
			const float* timeStamps = (float*)sampler.m_input->dataPtr;
			curve.StartTime = timeStamps[0];

			const float endTime = timeStamps[sampler.m_output->count - 1];
			curve.RangeScale = curve.NumSegments / (endTime - curve.StartTime);

			animSet.duration = std::max<float>(animSet.duration, endTime);

			// Append this curve data
			model.m_AnimationKeyFrameData.insert(
				model.m_AnimationKeyFrameData.end(),
				sampler.m_output->dataPtr,
				sampler.m_output->dataPtr + sampler.m_output->count * curve.KeyFrameStride * 4);

			model.m_AnimationCurves.push_back(curve);
		}
	}
}


void BuildSkins(glTFModelData& model, const glTF::Asset& asset)
{
	size_t numSkins = asset.m_skins.size();
	if (numSkins == 0)
		return;

	std::vector<std::pair<uint16_t, uint16_t>> skinMap;
	skinMap.reserve(asset.m_skins.size());

	for (const glTF::Skin& skin : asset.m_skins)
	{
		// Record offset and joint count
		uint16_t numJoints = (uint16_t)skin.joints.size();
		uint16_t curOffset = (uint16_t)model.m_JointIndices.size();
		skinMap.push_back(std::make_pair(curOffset, numJoints));

		// Append remapped joint indices
		for (glTF::Node* joint : skin.joints)
		{
			assert(joint->linearIdx >= 0);//Skin joint not present in node hierarchy
			model.m_JointIndices.push_back((uint16_t)joint->linearIdx);
		}

		// Append IBMs
		Matrix4* IBMstart = (Matrix4*)skin.inverseBindMatrices->dataPtr;
		Matrix4* IBMend = IBMstart + skin.inverseBindMatrices->count;
		assert(skin.inverseBindMatrices->count == numJoints);
		model.m_JointIBMs.insert(model.m_JointIBMs.end(), IBMstart, IBMend);
	}

	// Assign skinned meshes the proper joint offset and count
	for (Mesh* mesh : model.m_Meshes)
	{
		if (mesh->numJoints != 0)
		{
			std::pair<uint16_t, uint16_t> offsetAndCount = skinMap[mesh->startJoint];
			mesh->startJoint = offsetAndCount.first;
			mesh->numJoints = offsetAndCount.second;
		}
	}
}



bool GlareEngine::BuildModel(glTFModelData& model, const glTF::Asset& asset, int sceneIdx)
{
	BuildMaterials(model, asset);

	// Generate scene graph and meshes
	model.m_SceneGraph.resize(asset.m_nodes.size());
	const glTF::Scene* scene = sceneIdx < 0 ? asset.m_scene : &asset.m_scenes[sceneIdx];
	if (scene == nullptr)
	{
		EngineLog::AddLog(L"glTF model scene is null!");
		return false;
	}

	// Aggregate all of the vertex and index buffers in this unified buffer
	std::vector<byte>& bufferMemory = model.m_GeometryData;

	model.m_BoundingSphere = BoundingSphere(eZero);
	model.m_BoundingBox = AxisAlignedBox(eZero);
	uint32_t numNodes = WalkGraph(model.m_SceneGraph, model.m_BoundingSphere, model.m_BoundingBox, model.m_Meshes, bufferMemory, scene->nodes, 0, Matrix4(eIdentity));
	model.m_SceneGraph.resize(numNodes);

	//load animations
	BuildAnimations(model, asset);
	BuildSkins(model, asset);

	return true;
} 

bool GlareEngine::SaveModel(const std::wstring& filePath, const glTFModelData& model)
{
	std::ofstream outFile(filePath, std::ios::out | std::ios::binary);
	if (!outFile)
		return false;

	FileHeader header;
	std::memcpy(header.id, "Model", 5);
	header.version = CURRENT_MODEL_FILE_VERSION;
	header.numNodes = (uint32_t)model.m_SceneGraph.size();
	header.numMeshes = (uint32_t)model.m_Meshes.size();
	header.numMaterials = (uint32_t)model.m_MaterialConstants.size();
	header.meshDataSize = 0;
	for (const Mesh* mesh : model.m_Meshes)
		header.meshDataSize += (uint32_t)sizeof(Mesh) + (mesh->numDraws - 1) * (uint32_t)sizeof(Mesh::Draw);
	header.numTextures = (uint32_t)model.m_TextureNames.size();
	header.stringTableSize = 0;
	for (const std::string& str : model.m_TextureNames)
		header.stringTableSize += (uint32_t)str.size() + 1;
	header.geometrySize = (uint32_t)model.m_GeometryData.size();
	header.keyFrameDataSize = (uint32_t)model.m_AnimationKeyFrameData.size();
	header.numAnimationCurves = (uint32_t)model.m_AnimationCurves.size();
	header.numAnimations = (uint32_t)model.m_Animations.size();
	header.numJoints = (uint32_t)model.m_JointIndices.size();
	header.boundingSphere[0] = model.m_BoundingSphere.GetCenter().GetX();
	header.boundingSphere[1] = model.m_BoundingSphere.GetCenter().GetY();
	header.boundingSphere[2] = model.m_BoundingSphere.GetCenter().GetZ();
	header.boundingSphere[3] = model.m_BoundingSphere.GetRadius();
	header.minPos[0] = model.m_BoundingBox.GetMin().GetX();
	header.minPos[1] = model.m_BoundingBox.GetMin().GetY();
	header.minPos[2] = model.m_BoundingBox.GetMin().GetZ();
	header.maxPos[0] = model.m_BoundingBox.GetMax().GetX();
	header.maxPos[1] = model.m_BoundingBox.GetMax().GetY();
	header.maxPos[2] = model.m_BoundingBox.GetMax().GetZ();

	outFile.write((char*)&header, sizeof(FileHeader));
	outFile.write((char*)model.m_GeometryData.data(), header.geometrySize);
	outFile.write((char*)model.m_SceneGraph.data(), header.numNodes * sizeof(GraphNode));
	for (const Mesh* mesh : model.m_Meshes)
		outFile.write((char*)mesh, sizeof(Mesh) + (mesh->numDraws - 1) * sizeof(Mesh::Draw));
	outFile.write((char*)model.m_MaterialConstants.data(), header.numMaterials * sizeof(MaterialConstantData));
	outFile.write((char*)model.m_MaterialTextures.data(), header.numMaterials * sizeof(MaterialTextureData));
	for (uint32_t i = 0; i < header.numTextures; ++i)
		outFile << model.m_TextureNames[i] << '\0';
	outFile.write((char*)model.m_TextureOptions.data(), header.numTextures * sizeof(uint8_t));

	if (header.numAnimations > 0)
	{
		assert(header.keyFrameDataSize > 0 && header.numAnimationCurves > 0);
		outFile.write((char*)model.m_AnimationKeyFrameData.data(), header.keyFrameDataSize);
		outFile.write((char*)model.m_AnimationCurves.data(), header.numAnimationCurves * sizeof(AnimationCurve));
		outFile.write((char*)model.m_Animations.data(), header.numAnimations * sizeof(AnimationSet));
	}
	else
	{
		assert(header.keyFrameDataSize == 0 && header.numAnimationCurves == 0);
	}

	if (header.numJoints)
	{
		assert(header.numJoints == (uint32_t)model.m_JointIBMs.size());
		outFile.write((char*)model.m_JointIndices.data(), header.numJoints * sizeof(uint16_t));
		outFile.write((char*)model.m_JointIBMs.data(), header.numJoints * sizeof(Matrix4));
	}

	return true;
}


std::shared_ptr<Model> GlareEngine::LoadModel(const std::wstring& Path, bool forceRebuild)
{
	wstring filePath = Path;
	if (Path.rfind(':') == std::wstring::npos)
	{
		filePath = StringToWString(EngineGlobal::ModelAssetPath) + Path;
	}
	const std::wstring ModelFileName = RemoveExtension(filePath) + L".Model";
	const std::wstring fileName = RemoveBasePath(filePath);

	struct _stat64 sourceFileStat;
	struct _stat64 ModelFileStat;
	std::ifstream inFile;
	FileHeader header;

	bool sourceFileMissing = _wstat64(filePath.c_str(), &sourceFileStat) == -1;
	bool ModelFileMissing = _wstat64(ModelFileName.c_str(), &ModelFileStat) == -1;

	if (sourceFileMissing)
		forceRebuild = false;

	if (sourceFileMissing && ModelFileMissing)
	{
		EngineLog::AddLog(L"Error: Could not find %ws\n", fileName.c_str());
		return nullptr;
	}

	bool needBuild = forceRebuild;

	// Check if .Model file exists and it is newer than source file
	if (ModelFileMissing || !sourceFileMissing && sourceFileStat.st_mtime > ModelFileStat.st_mtime)
		needBuild = true;

	// Check if it's an older version of .Model
	if (!needBuild)
	{
		inFile = std::ifstream(ModelFileName, std::ios::in | std::ios::binary);
		inFile.read((char*)&header, sizeof(FileHeader));
		if (strncmp(header.id, "Model", 5) != 0 || header.version != CURRENT_MODEL_FILE_VERSION)
		{
			EngineLog::AddLog(L"Model version deprecated.Rebuilding %ws......\n", fileName.c_str());
			needBuild = true;
			inFile.close();
		}
	}

	if (needBuild)
	{
		if (sourceFileMissing)
		{
			EngineLog::AddLog(L"Error: Could not find %ws\n", fileName.c_str());
			return nullptr;
		}

		glTFModelData modelData;

		const std::wstring fileExt = ToLower(GetFileExtension(filePath));

		if (fileExt == L"gltf" || fileExt == L"glb")
		{
			glTF::Asset asset(filePath);
			if (!BuildModel(modelData, asset))
				return nullptr;
		}
		else
		{
			EngineLog::AddLog(L"Unsupported model file extension: %ws\n", fileExt.c_str());
			return nullptr;
		}

		if (!SaveModel(ModelFileName, modelData))
			return nullptr;

		inFile = std::ifstream(ModelFileName, std::ios::in | std::ios::binary);
		inFile.read((char*)&header, sizeof(FileHeader));

		for (Mesh* mesh: modelData.m_Meshes)
		{
			free(mesh);
		}
	}

	if (!inFile)
		return nullptr;

	assert(strncmp(header.id, "Model", 5) == 0 && header.version == CURRENT_MODEL_FILE_VERSION);

	std::wstring basePath = GetBasePath(filePath);

	std::shared_ptr<Model> model(new Model);

	model->m_NumNodes = header.numNodes;
	model->m_SceneGraph.reset(new GraphNode[header.numNodes]);
	model->m_NumMeshes = header.numMeshes;
	model->m_MeshData.reset(new uint8_t[header.meshDataSize]);

	//load geometry data
	{
		if (header.geometrySize > 0)
		{
			UploadBuffer modelData;
			modelData.Create(L"Model Data Upload", header.geometrySize);
			inFile.read((char*)modelData.Map(), header.geometrySize);
			modelData.Unmap();
			model->m_DataBuffer.Create(L"Model Data", header.geometrySize, 1, modelData);
		}

		inFile.read((char*)model->m_SceneGraph.get(), header.numNodes * sizeof(GraphNode));
		inFile.read((char*)model->m_MeshData.get(), header.meshDataSize);
	}

	// Read material
	{
		if (header.numMaterials > 0)
		{
			UploadBuffer materialConstants;
			materialConstants.Create(L"Material Constant Upload", header.numMaterials * sizeof(MaterialConstants));
			MaterialConstants* materialCBV = (MaterialConstants*)materialConstants.Map();
			for (uint32_t i = 0; i < header.numMaterials; ++i)
			{
				inFile.read((char*)materialCBV, sizeof(MaterialConstantData));
				materialCBV++;
			}
			materialConstants.Unmap();
			model->m_MaterialConstants.Create(L"Material Constants", header.numMaterials, sizeof(MaterialConstants), materialConstants);
		}

		// Read material texture and sampler properties so we can load the material
		std::vector<MaterialTextureData> materialTextures(header.numMaterials);
		inFile.read((char*)materialTextures.data(), header.numMaterials * sizeof(MaterialTextureData));

		std::vector<std::wstring> textureNames(header.numTextures);
		for (uint32_t i = 0; i < header.numTextures; ++i)
		{
			std::string utf8TextureName;
			std::getline(inFile, utf8TextureName, '\0');
			textureNames[i] = StringToWString(utf8TextureName);
		}

		std::vector<uint8_t> textureOptions(header.numTextures);
		inFile.read((char*)textureOptions.data(), header.numTextures * sizeof(uint8_t));

		LoadMaterials(*model, materialTextures, textureNames, textureOptions, basePath);
	}

	model->m_BoundingSphere = BoundingSphere(*(XMFLOAT4*)header.boundingSphere);
	model->m_BoundingBox = AxisAlignedBox(Vector3(*(XMFLOAT3*)header.minPos), Vector3(*(XMFLOAT3*)header.maxPos));

	// Load animation data
	{
		model->m_NumAnimations = header.numAnimations;

		if (header.numAnimations > 0)
		{
			assert(header.keyFrameDataSize > 0 && header.numAnimationCurves > 0);
			//load KeyFrameData
			model->m_KeyFrameData.reset(new uint8_t[header.keyFrameDataSize]);
			inFile.read((char*)model->m_KeyFrameData.get(), header.keyFrameDataSize);
			//load CurveData
			model->m_CurveData.reset(new AnimationCurve[header.numAnimationCurves]);
			inFile.read((char*)model->m_CurveData.get(), header.numAnimationCurves * sizeof(AnimationCurve));
			//load Animations
			model->m_Animations.reset(new AnimationSet[header.numAnimations]);
			inFile.read((char*)model->m_Animations.get(), header.numAnimations * sizeof(AnimationSet));
		}

		model->m_NumJoints = header.numJoints;

		if (header.numJoints > 0)
		{
			//load JointIndices
			model->m_JointIndices.reset(new uint16_t[header.numJoints]);
			inFile.read((char*)model->m_JointIndices.get(), header.numJoints * sizeof(uint16_t));
			//load JointIBMs
			model->m_JointIBMs.reset(new Matrix4[header.numJoints]);
			inFile.read((char*)model->m_JointIBMs.get(), header.numJoints * sizeof(Matrix4));
		}
	}
	return model;
}
