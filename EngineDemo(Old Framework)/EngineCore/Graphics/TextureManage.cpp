#include "TextureManage.h"
#include "Material.h"
#include <LoaderHelpers.h>

TextureManage::TextureManage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, UINT CbvSrvDescriptorSize, int NumFrameResources)
	:md3dDevice(d3dDevice),
	mCommandList(CommandList),
	gNumFrameResources(NumFrameResources),
	mCbvSrvDescriptorSize(CbvSrvDescriptorSize)
{
}

TextureManage::~TextureManage()
{
}

HRESULT FillInitData12(_In_ size_t width,
	_In_ size_t height,
	_In_ size_t depth,
	_In_ size_t mipCount,
	_In_ size_t arraySize,
	_In_ DXGI_FORMAT format,
	_In_ size_t maxsize,
	_In_ size_t bitSize,
	_In_reads_bytes_(bitSize) const uint8_t* bitData,
	_Out_ size_t& twidth,
	_Out_ size_t& theight,
	_Out_ size_t& tdepth,
	_Out_ size_t& skipMip,
	_Out_writes_(mipCount* arraySize) D3D12_SUBRESOURCE_DATA* initData
)
{
	if (!bitData || !initData)
	{
		return E_POINTER;
	}

	skipMip = 0;
	twidth = 0;
	theight = 0;
	tdepth = 0;

	size_t NumBytes = 0;
	size_t RowBytes = 0;
	const uint8_t* pSrcBits = bitData;
	const uint8_t* pEndBits = bitData + bitSize;

	size_t index = 0;
	for (size_t j = 0; j < arraySize; j++)
	{
		size_t w = width;
		size_t h = height;
		size_t d = depth;
		for (size_t i = 0; i < mipCount; i++)
		{
			DirectX::LoaderHelpers::GetSurfaceInfo(w,
				h,
				format,
				&NumBytes,
				&RowBytes,
				nullptr
			);

			if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
			{
				if (!twidth)
				{
					twidth = w;
					theight = h;
					tdepth = d;
				}

				assert(index < mipCount* arraySize);
				_Analysis_assume_(index < mipCount* arraySize);
				initData[index]./*pSysMem*/pData = (const void*)pSrcBits;
				initData[index]./*SysMemPitch*/RowPitch = static_cast<UINT>(RowBytes);
				initData[index]./*SysMemSlicePitch*/SlicePitch = static_cast<UINT>(NumBytes);
				++index;
			}
			else if (!j)
			{
				// Count number of skipped mipmaps (first item only)
				++skipMip;
			}

			if (pSrcBits + (NumBytes * d) > pEndBits)
			{
				return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
			}

			pSrcBits += NumBytes * d;

			w = w >> 1;
			h = h >> 1;
			d = d >> 1;
			if (w == 0)
			{
				w = 1;
			}
			if (h == 0)
			{
				h = 1;
			}
			if (d == 0)
			{
				d = 1;
			}
		}
	}

	return (index > 0) ? S_OK : E_FAIL;
}

HRESULT TextureManage::CreateD3DResources12(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	_In_ uint32_t resDim,
	_In_ size_t width,
	_In_ size_t height,
	_In_ size_t depth,
	_In_ size_t mipCount,
	_In_ size_t arraySize,
	_In_ DXGI_FORMAT format,
	_In_ bool forceSRGB,
	_In_ bool isCubeMap,
	_In_reads_opt_(mipCount* arraySize) D3D12_SUBRESOURCE_DATA* initData,
	ComPtr<ID3D12Resource>& texture,
	ComPtr<ID3D12Resource>& textureUploadHeap)
{
	if (device == nullptr)
		return E_POINTER;

	if (forceSRGB)
		format = DirectX::LoaderHelpers::MakeSRGB(format);

	HRESULT hr = E_FAIL;
	switch (resDim)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
	{
		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = width;
		texDesc.Height = (uint32_t)height;
		texDesc.DepthOrArraySize = (depth > 1) ? (uint16_t)depth : (uint16_t)arraySize;
		texDesc.MipLevels = (uint16_t)mipCount;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		hr = device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&texture)
		);

		if (FAILED(hr))
		{
			texture = nullptr;
			return hr;
		}
		else
		{
			const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, num2DSubresources);

			D3D12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
			hr = device->CreateCommittedResource(
				&HeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&Desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap));
			if (FAILED(hr))
			{
				texture = nullptr;
				return hr;
			}
			else
			{
				D3D12_RESOURCE_BARRIER Barriers = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),
					D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				cmdList->ResourceBarrier(1, &Barriers);

				// Use Heap-allocating UpdateSubresources implementation for variable number of subresources (which is the case for textures).
				UpdateSubresources(cmdList, texture.Get(), textureUploadHeap.Get(), 0, 0, num2DSubresources, initData);

				Barriers = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				cmdList->ResourceBarrier(1, &Barriers);
			}
		}
	} break;
	}

	return hr;
}

HRESULT TextureManage::CreateTextureFromDDS12(
	_In_ ID3D12Device* device,
	_In_opt_ ID3D12GraphicsCommandList* cmdList,
	_In_ const DDS_HEADER* header,
	_In_reads_bytes_(bitSize) const uint8_t* bitData,
	_In_ size_t bitSize,
	_In_ size_t maxsize,
	_In_ bool forceSRGB,
	ComPtr<ID3D12Resource>& texture,
	ComPtr<ID3D12Resource>& textureUploadHeap)
{
	HRESULT hr = S_OK;

	UINT width = header->width;
	UINT height = header->height;
	UINT depth = header->depth;

	uint32_t resDim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
	UINT arraySize = 1;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	bool isCubeMap = false;

	size_t mipCount = header->mipMapCount;
	if (0 == mipCount) mipCount = 1;

	if ((header->ddspf.flags & DDS_FOURCC) && (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
	{
		auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>((const char*)header + sizeof(DDS_HEADER));

		arraySize = d3d10ext->arraySize;
		if (arraySize == 0)
			return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);

		switch (d3d10ext->dxgiFormat)
		{
		case DXGI_FORMAT_AI44:
		case DXGI_FORMAT_IA44:
		case DXGI_FORMAT_P8:
		case DXGI_FORMAT_A8P8:
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

		default:
			if (DirectX::LoaderHelpers::BitsPerPixel(d3d10ext->dxgiFormat) == 0)
				return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}

		format = d3d10ext->dxgiFormat;

		switch (d3d10ext->resourceDimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			if ((header->flags & DDS_HEIGHT) && height != 1)
				return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
			height = depth = 1;
			break;

		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			if (d3d10ext->miscFlag & 0x4L)
			{
				arraySize *= 6;
				isCubeMap = true;
			}
			depth = 1;
			break;

		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
				return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
			if (arraySize > 1)
				return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
			break;

		default:
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}

		switch (d3d10ext->resourceDimension)
		{
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			break;
		}
	}
	else
	{
		format = DirectX::LoaderHelpers::GetDXGIFormat(header->ddspf);

		if (format == DXGI_FORMAT_UNKNOWN)
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

		if (header->flags & DDS_HEADER_FLAGS_VOLUME)
		{
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		}
		else
		{
			if (header->caps2 & DDS_CUBEMAP)
			{
				if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
					return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
				arraySize = 6;
				isCubeMap = true;
			}

			depth = 1;
			resDim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		}

		assert(DirectX::LoaderHelpers::BitsPerPixel(format) != 0);
	}

	// Bound sizes (for security purposes we don't trust DDS file metadata larger than the D3D 11.x hardware requirements)
	if (mipCount > D3D12_REQ_MIP_LEVELS)
	{
		return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	}

	switch (resDim)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		if ((arraySize > D3D12_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
			(width > D3D12_REQ_TEXTURE1D_U_DIMENSION))
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (isCubeMap)
		{
			// This is the right bound because we set arraySize to (NumCubes*6) above
			if ((arraySize > D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
				(width > D3D12_REQ_TEXTURECUBE_DIMENSION) ||
				(height > D3D12_REQ_TEXTURECUBE_DIMENSION))
			{
				return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
			}
		}
		else if ((arraySize > D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
			(width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
			(height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION))
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
		break;

	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		if ((arraySize > 1) ||
			(width > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
			(height > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
			(depth > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
		{
			return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		}
		break;

	default:
		return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	}

	// Create the texture
	std::unique_ptr<D3D12_SUBRESOURCE_DATA[]> initData(
		new (std::nothrow) D3D12_SUBRESOURCE_DATA[mipCount * arraySize]
	);

	if (!initData)
	{
		return E_OUTOFMEMORY;
	}

	size_t skipMip = 0;
	size_t twidth = 0;
	size_t theight = 0;
	size_t tdepth = 0;

	hr = FillInitData12(
		width, height, depth, mipCount, arraySize, format, maxsize, bitSize, bitData,
		twidth, theight, tdepth, skipMip, initData.get()
	);

	if (SUCCEEDED(hr))
	{
		hr = CreateD3DResources12(
			device, cmdList,
			resDim, twidth, theight, tdepth,
			mipCount - skipMip,
			arraySize,
			format,
			forceSRGB, // forceSRGB
			isCubeMap,
			initData.get(),
			texture,
			textureUploadHeap);
	}

	return hr;
}

HRESULT TextureManage::CreateDDSTextureFromFile12(_In_ ID3D12Device* device,
	_In_ ID3D12GraphicsCommandList* cmdList,
	_In_z_ const wchar_t* szFileName,
	_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
	_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
	_In_ bool forceSRGB,
	_In_ size_t maxsize,
	_Out_opt_ DDS_ALPHA_MODE* alphaMode)
{
	if (texture)
	{
		texture = nullptr;
	}
	if (textureUploadHeap)
	{
		textureUploadHeap = nullptr;
	}
	if (alphaMode)
	{
		*alphaMode = DDS_ALPHA_MODE_UNKNOWN;
	}

	if (!device || !szFileName)
	{
		return E_INVALIDARG;
	}

	DDS_HEADER* header = nullptr;
	uint8_t* bitData = nullptr;
	size_t bitSize = 0;

	std::unique_ptr<uint8_t[]> ddsData;
	HRESULT hr = DirectX::LoaderHelpers::LoadTextureDataFromFile(szFileName, ddsData, (const DDS_HEADER**)(&header), (const uint8_t**)(&bitData), &bitSize);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = CreateTextureFromDDS12(device, cmdList, header,
		bitData, bitSize, maxsize, forceSRGB, texture, textureUploadHeap);

	if (SUCCEEDED(hr))
	{
		if (alphaMode)
			*alphaMode = DirectX::LoaderHelpers::GetAlphaMode(header);
	}

	return hr;
}

void TextureManage::CreateTexture(std::wstring name, std::wstring filename, bool ForceSRGB)
{
	// 它已经存在了吗？
	std::unique_ptr<Texture> tex = std::make_unique<Texture>();
	if (mTextures.find(name) == mTextures.end())
	{
		if (!CheckFileExist(filename))
		{
			MessageBox(nullptr, std::wstring(L"文件" + filename + L"不存在！").c_str(), L"错误", MB_OK);
			return;
		}

		HRESULT hr = S_OK;

		ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice,
			mCommandList, filename.c_str(),
			tex.get()->Resource, tex.get()->UploadHeap, ForceSRGB));

		if (hr != S_OK)
		{
			ThrowIfFailed(hr);
			md3dDevice->GetDeviceRemovedReason();
		}

		mTextures[name] = std::move(tex);
	}
}

std::unique_ptr<Texture>& TextureManage::GetTexture(std::wstring name, bool ForceSRGB)
{
	if (mTextures.find(name) == mTextures.end())
	{
		CreateTexture(name, RootFilePath + name + L".dds", ForceSRGB);
	}
	return mTextures[name];
}

std::unique_ptr<Texture>& TextureManage::GetModelTexture(std::wstring name, bool ForceSRGB)
{
	if (mTextures.find(name) == mTextures.end())
	{
		CreateTexture(name, name + L".dds", ForceSRGB);
	}
	return mTextures[name];
}

void TextureManage::CreatePBRSRVinDescriptorHeap(std::vector<ID3D12Resource*> TexResource, int* SRVIndex, CD3DX12_CPU_DESCRIPTOR_HANDLE* hDescriptor, std::wstring MaterialName)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (int i = 0; i < TexResource.size(); ++i)
	{
		srvDesc.Format = TexResource[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = TexResource[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(TexResource[i], &srvDesc, *hDescriptor);
		Materials::GetMaterialInstance()->GetMaterial(MaterialName)->PBRSrvHeapIndex[i] = (*SRVIndex)++;
		// next descriptor
		hDescriptor->Offset(1, mCbvSrvDescriptorSize);
	}
}
