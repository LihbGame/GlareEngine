#include "MeshBuilder.h"
#include "TextureConvert.h"
#include "glTF.h"
#include "Model.h"
#include "IndexOptimize.h"
#include "VectorMath.h"
#include "DirectXMesh.h"
#include "EngineLog.h"

using namespace DirectX;
using namespace glTF;
using namespace GlareEngine::Math;


static DXGI_FORMAT JointIndexFormat(const Accessor& accessor)
{
	switch (accessor.componentType)
	{
	case Accessor::eUnsignedByte:  return DXGI_FORMAT_R8G8B8A8_UINT;
	case Accessor::eUnsignedShort: return DXGI_FORMAT_R16G16B16A16_UINT;
	default:
		EngineLog::AddLog(L"Invalid joint index format");
		return DXGI_FORMAT_UNKNOWN;
	}
}

static DXGI_FORMAT AccessorFormat(const Accessor& accessor)
{
	switch (accessor.componentType)
	{
	case Accessor::eUnsignedByte:
		switch (accessor.type)
		{
		case Accessor::eScalar: return DXGI_FORMAT_R8_UNORM;
		case Accessor::eVec2:   return DXGI_FORMAT_R8G8_UNORM;
		default:                return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
	case Accessor::eUnsignedShort:
		switch (accessor.type)
		{
		case Accessor::eScalar: return DXGI_FORMAT_R16_UNORM;
		case Accessor::eVec2:   return DXGI_FORMAT_R16G16_UNORM;
		default:                return DXGI_FORMAT_R16G16B16A16_UNORM;
		}
	case Accessor::eFloat:
		switch (accessor.type)
		{
		case Accessor::eScalar: return DXGI_FORMAT_R32_FLOAT;
		case Accessor::eVec2:   return DXGI_FORMAT_R32G32_FLOAT;
		case Accessor::eVec3:   return DXGI_FORMAT_R32G32B32_FLOAT;
		default:                return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
	default:
		EngineLog::AddLog(L"Invalid accessor format");
		return DXGI_FORMAT_UNKNOWN;
	}
}


void GlareEngine::BuildMesh(Primitive& outPrimitive, const glTF::Primitive& inPrimitive, const Math::Matrix4& localToObject)
{
	assert(inPrimitive.attributes[glTF::Primitive::ePosition] != nullptr); //Must have Position!
	uint32_t vertexCount = inPrimitive.attributes[glTF::Primitive::ePosition]->count;

	void* indices = nullptr;
	uint32_t indexCount;
	bool b32BitIndices;
	uint32_t maxIndex = inPrimitive.maxIndex;

	if (inPrimitive.indices == nullptr)
	{
		assert(inPrimitive.mode == 4);//Impossible primitive topology (TRIANGLE LIST) when lacking indices

		indexCount = vertexCount * 3;
		maxIndex = indexCount - 1;
		//if we use 16 or 32 bit index
		if (indexCount > 0xFFFF)
		{
			b32BitIndices = true;
			outPrimitive.IB = std::make_shared<std::vector<byte>>(4 * indexCount);
			indices = outPrimitive.IB->data();
			uint32_t* tmp = (uint32_t*)indices;
			for (uint32_t i = 0; i < indexCount; ++i)
				tmp[i] = i;
		}
		else
		{
			b32BitIndices = false;
			outPrimitive.IB = std::make_shared<std::vector<byte>>(2 * indexCount);
			indices = outPrimitive.IB->data();
			uint16_t* tmp = (uint16_t*)indices;
			for (uint16_t i = 0; i < indexCount; ++i)
				tmp[i] = i;
		}
	}
	else
	{
		switch (inPrimitive.mode)
		{
		default:
		case 0: // POINT LIST
		case 1: // LINE LIST
		case 2: // LINE LOOP
		case 3: // LINE STRIP
			EngineLog::AddLog(L"Found unsupported primitive topology\n");
			return;
		case 4: // TRIANGLE LIST
			break;
		case 5: // TODO: Convert TRIANGLE STRIP
		case 6: // TODO: Convert TRIANGLE FAN
			EngineLog::AddLog(L"Found an index buffer that needs to be converted to a triangle list\n");
			return;
		}

		indices = inPrimitive.indices->dataPtr;
		indexCount = inPrimitive.indices->count;
		if (maxIndex == 0)
		{
			if (inPrimitive.indices->componentType == Accessor::eUnsignedInt)
			{
				uint32_t* ib = (uint32_t*)inPrimitive.indices->dataPtr;
				for (uint32_t k = 0; k < indexCount; ++k)
					maxIndex = std::max<uint32_t>(ib[k], maxIndex);
			}
			else
			{
				uint16_t* ib = (uint16_t*)inPrimitive.indices->dataPtr;
				for (uint32_t k = 0; k < indexCount; ++k)
					maxIndex = std::max<uint32_t>(ib[k], maxIndex);
			}
		}

		b32BitIndices = maxIndex > 0xFFFF;
		uint32_t indexSize = b32BitIndices ? 4 : 2;
		outPrimitive.IB = std::make_shared<std::vector<byte>>(indexSize * indexCount);
		if (b32BitIndices)
		{
			assert(inPrimitive.indices->componentType == Accessor::eUnsignedInt);
			OptimizeFaces((uint32_t*)inPrimitive.indices->dataPtr, inPrimitive.indices->count, (uint32_t*)outPrimitive.IB->data(), 64);
		}
		else if (inPrimitive.indices->componentType == Accessor::eUnsignedShort)
		{
			OptimizeFaces((uint16_t*)inPrimitive.indices->dataPtr, inPrimitive.indices->count, (uint16_t*)outPrimitive.IB->data(), 64);
		}
		else
		{
			OptimizeFaces((uint32_t*)inPrimitive.indices->dataPtr, inPrimitive.indices->count, (uint16_t*)outPrimitive.IB->data(), 64);
		}
		indices = outPrimitive.IB->data();
	}

	assert(maxIndex > 0);

	const bool HasNormals = inPrimitive.attributes[glTF::Primitive::eNormal] != nullptr;
	const bool HasTangents = inPrimitive.attributes[glTF::Primitive::eTangent] != nullptr;
	const bool HasUV0 = inPrimitive.attributes[glTF::Primitive::eTexcoord0] != nullptr;
	const bool HasUV1 = inPrimitive.attributes[glTF::Primitive::eTexcoord1] != nullptr;
	const bool HasJoints = inPrimitive.attributes[glTF::Primitive::eJoints0] != nullptr;
	const bool HasWeights = inPrimitive.attributes[glTF::Primitive::eWeights0] != nullptr;
	const bool HasSkin = HasJoints && HasWeights;

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputElements;
	InputElements.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, glTF::Primitive::ePosition });
	if (HasNormals)
	{
		InputElements.push_back({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, glTF::Primitive::eNormal });
	}
	if (HasTangents)
	{
		InputElements.push_back({ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  glTF::Primitive::eTangent });
	}
	if (HasUV0)
	{
		InputElements.push_back({ "TEXCOORD", 0,
			AccessorFormat(*inPrimitive.attributes[glTF::Primitive::eTexcoord0]),
			glTF::Primitive::eTexcoord0 });
	}
	if (HasUV1)
	{
		InputElements.push_back({ "TEXCOORD", 1,
			AccessorFormat(*inPrimitive.attributes[glTF::Primitive::eTexcoord1]),
			glTF::Primitive::eTexcoord1 });
	}
	if (HasSkin)
	{
		InputElements.push_back({ "BLENDINDICES", 0,
			JointIndexFormat(*inPrimitive.attributes[glTF::Primitive::eJoints0]),
			glTF::Primitive::eJoints0 });
		InputElements.push_back({ "BLENDWEIGHT", 0,
			AccessorFormat(*inPrimitive.attributes[glTF::Primitive::eWeights0]),
			glTF::Primitive::eWeights0 });
	}

	VBReader vbr;
	vbr.Initialize({ InputElements.data(), (uint32_t)InputElements.size() });

	for (uint32_t i = 0; i < glTF::Primitive::eNumAttribs; ++i)
	{
		Accessor* attrib = inPrimitive.attributes[i];
		if (attrib)
		{
			vbr.AddStream(attrib->dataPtr, vertexCount, i, attrib->stride);
		}
	}

	const glTF::Material& material = *inPrimitive.material;

	std::unique_ptr<XMFLOAT3[]> position;
	std::unique_ptr<XMFLOAT3[]> normal;
	std::unique_ptr<XMFLOAT4[]> tangent;
	std::unique_ptr<XMFLOAT2[]> texcoord0;
	std::unique_ptr<XMFLOAT2[]> texcoord1;
	std::unique_ptr<XMFLOAT4[]> joints;
	std::unique_ptr<XMFLOAT4[]> weights;
	position.reset(new XMFLOAT3[vertexCount]);
	normal.reset(new XMFLOAT3[vertexCount]);

	ThrowIfFailed(vbr.Read(position.get(), "POSITION", 0, vertexCount));
	{
		// Local space bounds
		Vector3 sphereCenterLS = (Vector3(*(XMFLOAT3*)inPrimitive.minPos) + Vector3(*(XMFLOAT3*)inPrimitive.maxPos)) * 0.5f;
		Scalar maxRadiusLSSq(eZero);

		// Object space bounds
		Vector3 sphereCenterOS = Vector3(Vector4(sphereCenterLS) * localToObject);
		Scalar maxRadiusOSSq(eZero);

		outPrimitive.m_BBoxLS = AxisAlignedBox(eZero);
		outPrimitive.m_BBoxOS = AxisAlignedBox(eZero);

		for (uint32_t v = 0; v < vertexCount; ++v)
		{
			Vector3 positionLS = Vector3(position[v]);
			maxRadiusLSSq = Max(maxRadiusLSSq, LengthSquare(sphereCenterLS - positionLS));

			outPrimitive.m_BBoxLS.AddPoint(positionLS);

			Vector3 positionOS = Vector3(Vector4(positionLS) * localToObject);
			maxRadiusOSSq = Max(maxRadiusOSSq, LengthSquare(sphereCenterOS - positionOS));

			outPrimitive.m_BBoxOS.AddPoint(positionOS);
		}

		outPrimitive.m_BoundsLS = Math::BoundingSphere(sphereCenterLS, Sqrt(maxRadiusLSSq));
		outPrimitive.m_BoundsOS = Math::BoundingSphere(sphereCenterOS, Sqrt(maxRadiusOSSq));
		assert(outPrimitive.m_BoundsOS.GetRadius() > 0.0f);
	}

	if (HasNormals)
	{
		ThrowIfFailed(vbr.Read(normal.get(), "NORMAL", 0, vertexCount));
	}
	else
	{
		const size_t faceCount = indexCount / 3;

		if (b32BitIndices)
			ComputeNormals((const uint32_t*)indices, faceCount, position.get(), vertexCount, CNORM_DEFAULT, normal.get());
		else
			ComputeNormals((const uint16_t*)indices, faceCount, position.get(), vertexCount, CNORM_DEFAULT, normal.get());
	}

	if (HasUV0)
	{
		texcoord0.reset(new XMFLOAT2[vertexCount]);
		ThrowIfFailed(vbr.Read(texcoord0.get(), "TEXCOORD", 0, vertexCount));
	}

	if (HasUV1)
	{
		texcoord1.reset(new XMFLOAT2[vertexCount]);
		ThrowIfFailed(vbr.Read(texcoord1.get(), "TEXCOORD", 1, vertexCount));
	}

	if (HasTangents)
	{
		tangent.reset(new XMFLOAT4[vertexCount]);
		ThrowIfFailed(vbr.Read(tangent.get(), "TANGENT", 0, vertexCount));
	}
	else
	{
		assert(maxIndex < vertexCount);
		assert(indexCount % 3 == 0);

		HRESULT hr = S_OK;

		if (HasUV0 && material.normalUV == 0)
		{
			tangent.reset(new XMFLOAT4[vertexCount]);
			if (b32BitIndices)
			{
				hr = ComputeTangentFrame((uint32_t*)indices, indexCount / 3, position.get(), normal.get(), texcoord0.get(),
					vertexCount, tangent.get());
			}
			else
			{
				hr = ComputeTangentFrame((uint16_t*)indices, indexCount / 3, position.get(), normal.get(), texcoord0.get(),
					vertexCount, tangent.get());
			}
		}
		else if (HasUV1 && material.normalUV == 1)
		{
			tangent.reset(new XMFLOAT4[vertexCount]);
			if (b32BitIndices)
			{
				hr = ComputeTangentFrame((uint32_t*)indices, indexCount / 3, position.get(), normal.get(), texcoord1.get(),
					vertexCount, tangent.get());
			}
			else
			{
				hr = ComputeTangentFrame((uint16_t*)indices, indexCount / 3, position.get(), normal.get(), texcoord1.get(),
					vertexCount, tangent.get());
			}
		}

		ThrowIfFailed(hr);//Error generating a tangent frame
	}

	if (HasSkin)
	{
		joints.reset(new XMFLOAT4[vertexCount]);
		weights.reset(new XMFLOAT4[vertexCount]);
		ThrowIfFailed(vbr.Read(joints.get(), "BLENDINDICES", 0, vertexCount));
		ThrowIfFailed(vbr.Read(weights.get(), "BLENDWEIGHT", 0, vertexCount));
	}

	// Use VBWriter to generate a new, interleaved and compressed vertex buffer
	std::vector<D3D12_INPUT_ELEMENT_DESC> OutputElements;

	outPrimitive.psoFlags = PSOFlags::eHasPosition | PSOFlags::eHasNormal;
	OutputElements.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
	OutputElements.push_back({ "NORMAL", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT });
	if (tangent.get())
	{
		OutputElements.push_back({ "TANGENT", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT });
		outPrimitive.psoFlags |= PSOFlags::eHasTangent;
	}
	if (texcoord0.get())
	{
		OutputElements.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
		outPrimitive.psoFlags |= PSOFlags::eHasUV0;
	}
	if (texcoord1.get())
	{
		OutputElements.push_back({ "TEXCOORD", 1, DXGI_FORMAT_R16G16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
		outPrimitive.psoFlags |= PSOFlags::eHasUV1;
	}
	if (HasSkin)
	{
		OutputElements.push_back({ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
		OutputElements.push_back({ "BLENDWEIGHT", 0, DXGI_FORMAT_R16G16B16A16_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT });
		outPrimitive.psoFlags |= PSOFlags::eHasSkin;
	}
	if (material.alphaBlend)
		outPrimitive.psoFlags |= PSOFlags::eAlphaBlend;
	if (material.alphaTest)
		outPrimitive.psoFlags |= PSOFlags::eAlphaTest;
	if (material.twoSided)
		outPrimitive.psoFlags |= PSOFlags::eTwoSided;

	D3D12_INPUT_LAYOUT_DESC layout = { OutputElements.data(), (uint32_t)OutputElements.size() };

	VBWriter vbw;
	vbw.Initialize(layout);

	uint32_t offsets[10];
	uint32_t strides[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	ComputeInputLayout(layout, offsets, strides);
	uint32_t stride = strides[0];

	outPrimitive.VB = std::make_shared<std::vector<byte>>(stride * vertexCount);
	ThrowIfFailed(vbw.AddStream(outPrimitive.VB->data(), vertexCount, 0, stride));

	vbw.Write(position.get(), "POSITION", 0, vertexCount);
	vbw.Write(normal.get(), "NORMAL", 0, vertexCount, true);
	if (tangent.get())
		vbw.Write(tangent.get(), "TANGENT", 0, vertexCount, true);
	if (texcoord0.get())
		vbw.Write(texcoord0.get(), "TEXCOORD", 0, vertexCount);
	if (texcoord1.get())
		vbw.Write(texcoord1.get(), "TEXCOORD", 1, vertexCount);
	if (HasSkin)
	{
		vbw.Write(joints.get(), "BLENDINDICES", 0, vertexCount);
		vbw.Write(weights.get(), "BLENDWEIGHT", 0, vertexCount);
	}


	// Now write a VB for positions only (or positions and UV when alpha testing)
	uint32_t depthStride = 12;
	std::vector<D3D12_INPUT_ELEMENT_DESC> DepthElements;
	DepthElements.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
	if (material.alphaTest)
	{
		depthStride += 4;
		DepthElements.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
	}
	if (HasSkin)
	{
		depthStride += 16;
		DepthElements.push_back({ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT });
		DepthElements.push_back({ "BLENDWEIGHT", 0, DXGI_FORMAT_R16G16B16A16_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT });
	}

	VBWriter dvbw;
	dvbw.Initialize({ DepthElements.data(), (uint32_t)DepthElements.size() });

	outPrimitive.DepthVB = std::make_shared<std::vector<byte>>(depthStride * vertexCount);
	ThrowIfFailed(dvbw.AddStream(outPrimitive.DepthVB->data(), vertexCount, 0, depthStride));

	dvbw.Write(position.get(), "POSITION", 0, vertexCount);
	if (material.alphaTest)
	{
		dvbw.Write(material.baseColorUV ? texcoord1.get() : texcoord0.get(), "TEXCOORD", 0, vertexCount);
	}
	if (HasSkin)
	{
		dvbw.Write(joints.get(), "BLENDINDICES", 0, vertexCount);
		dvbw.Write(weights.get(), "BLENDWEIGHT", 0, vertexCount);
	}

	assert(material.index < 0x8000);//Only 15-bit material indices allowed

	outPrimitive.vertexStride = (uint16_t)stride;
	outPrimitive.index32 = b32BitIndices ? 1 : 0;
	outPrimitive.materialIdx = material.index;

	outPrimitive.primCount = indexCount;

}
