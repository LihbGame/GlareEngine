
#include "EngineUtility.h"
#include "EngineLog.h"
#include "Math/Common.h"
#include <comdef.h>
#include <fstream>
#include <locale>

#define RandomSize 1024


using Microsoft::WRL::ComPtr;

namespace GlareEngine
{
	DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
		ErrorCode(hr),
		FunctionName(functionName),
		Filename(filename),
		LineNumber(lineNumber)
	{
	}

	bool EngineUtility::IsKeyDown(int vkeyCode)
	{
		return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
	}

	ComPtr<ID3DBlob> EngineUtility::LoadBinary(const std::wstring& filename)
	{
		std::ifstream fin(filename, std::ios::binary);

		fin.seekg(0, std::ios_base::end);
		std::ifstream::pos_type size = (int)fin.tellg();
		fin.seekg(0, std::ios_base::beg);

		ComPtr<ID3DBlob> blob;
		ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

		fin.read((char*)blob->GetBufferPointer(), size);
		fin.close();

		return blob;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> EngineUtility::CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
	{
		ComPtr<ID3D12Resource> defaultBuffer;

		CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 

		HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		buffer = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = byteSize;
		subResourceData.SlicePitch = subResourceData.RowPitch;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		CD3DX12_RESOURCE_BARRIER CommonToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &CommonToCopyDest);
		UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

		CD3DX12_RESOURCE_BARRIER CopyDestToRead = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
		cmdList->ResourceBarrier(1, &CopyDestToRead);

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.


		return defaultBuffer;
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> EngineUtility::CreateRandomTexture1DSRV(ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
	{
		// 
		// Create the random data.
		//
		XMFLOAT4 randomValues[RandomSize];

		for (int i = 0; i < RandomSize; ++i)
		{
			randomValues[i].x = MathHelper::RandF(-1.0f, 1.0f);
			randomValues[i].y = MathHelper::RandF(-1.0f, 1.0f);
			randomValues[i].z = MathHelper::RandF(-1.0f, 1.0f);
			randomValues[i].w = MathHelper::RandF(-1.0f, 1.0f);
		}



		ComPtr<ID3D12Resource> defaultBuffer;

		CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Tex1D(DXGI_FORMAT_R32G32B32A32_FLOAT, RandomSize);

		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap.
		HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		buffer = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(defaultBuffer.Get(), 0, 1));
		ThrowIfFailed(device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA initData;
		initData.pData = randomValues;
		initData.RowPitch = RandomSize * sizeof(XMFLOAT4);
		initData.SlicePitch = 0;


		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		CD3DX12_RESOURCE_BARRIER CommonToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &CommonToCopyDest);

		UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &initData);

		CD3DX12_RESOURCE_BARRIER CopyDestToPixelShader = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &CopyDestToPixelShader);

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.


		return defaultBuffer;
	}




	Microsoft::WRL::ComPtr<ID3D12Resource> EngineUtility::CreateDefault2DTexture(
		ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize, DXGI_FORMAT format,
		UINT64 width, UINT height,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
	{
		ComPtr<ID3D12Resource> defaultBuffer;

		CD3DX12_HEAP_PROPERTIES HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height);
		// Create the actual default buffer resource.
		ThrowIfFailed(device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

		// In order to copy CPU memory data into our default buffer, we need to create
		// an intermediate upload heap. 
		HeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		buffer = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(defaultBuffer.Get(), 0, 1));
		ThrowIfFailed(device->CreateCommittedResource(
			&HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


		// Describe the data we want to copy into the default buffer.
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = byteSize / height;
		subResourceData.SlicePitch = byteSize;

		// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
		// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
		// the intermediate upload heap data will be copied to mBuffer.
		CD3DX12_RESOURCE_BARRIER CommonToCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
		cmdList->ResourceBarrier(1, &CommonToCopyDest);
		UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

		CD3DX12_RESOURCE_BARRIER CopyDestToPixelShader = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &CopyDestToPixelShader);

		// Note: uploadBuffer has to be kept alive after the above function calls because
		// the command list has not been executed yet that performs the actual copy.
		// The caller can Release the uploadBuffer after it knows the copy has been executed.


		return defaultBuffer;
	}

	ComPtr<ID3DBlob> EngineUtility::CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target)
	{
		UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
		compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		HRESULT hr = S_OK;


		ComPtr<ID3DBlob> byteCode = nullptr;
		ComPtr<ID3DBlob> errors;
		hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

		if (errors != nullptr)
			OutputDebugStringA((char*)errors->GetBufferPointer());

		ThrowIfFailed(hr);

		return byteCode;
	}

	void EngineUtility::CreateWICTextureFromFile(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, ID3D12Resource** tex, ID3D12Resource** Uploadtex, wstring filename)
	{
		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource;
		ThrowIfFailed(LoadWICTextureFromFileEx(d3dDevice, filename.c_str(), 0,
			D3D12_RESOURCE_FLAG_NONE, WIC_LOADER_MIP_AUTOGEN, tex,
			decodedData, subresource));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(*tex, 0, 1);

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

		auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

		// Create the GPU upload buffer.
		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(Uploadtex)));

		UpdateSubresources(CommandList, *tex, *Uploadtex,
			0, 0, 1, &subresource);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(*tex,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandList->ResourceBarrier(1, &barrier);
	}

	void EngineUtility::CreateWICTextureFromMemory(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, ID3D12Resource** tex, ID3D12Resource** Uploadtex, unsigned char* data, int size)
	{
		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource;
		ThrowIfFailed(LoadWICTextureFromMemoryEx(d3dDevice, data, 0, size,
			D3D12_RESOURCE_FLAG_NONE, WIC_LOADER_MIP_AUTOGEN, tex,
			decodedData, subresource));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(*tex, 0, 1);

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

		auto desc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

		// Create the GPU upload buffer.
		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(Uploadtex)));

		UpdateSubresources(CommandList, *tex, *Uploadtex,
			0, 0, 1, &subresource);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(*tex,
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandList->ResourceBarrier(1, &barrier);
	}

	std::wstring DxException::ToString()const
	{
		// Get the string description of the error code.
		_com_error err(ErrorCode);
		std::wstring msg = err.ErrorMessage();

		return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
	}

	void ExtractFrustumPlanes(XMFLOAT4 planes[6], CXMMATRIX T)
	{
		XMFLOAT4X4 M;
		XMStoreFloat4x4(&M, T);
		//
		// Left
		//
		planes[0].x = M(0, 3) + M(0, 0);
		planes[0].y = M(1, 3) + M(1, 0);
		planes[0].z = M(2, 3) + M(2, 0);
		planes[0].w = M(3, 3) + M(3, 0);

		//
		// Right
		//
		planes[1].x = M(0, 3) - M(0, 0);
		planes[1].y = M(1, 3) - M(1, 0);
		planes[1].z = M(2, 3) - M(2, 0);
		planes[1].w = M(3, 3) - M(3, 0);

		//
		// Bottom
		//
		planes[2].x = M(0, 3) + M(0, 1);
		planes[2].y = M(1, 3) + M(1, 1);
		planes[2].z = M(2, 3) + M(2, 1);
		planes[2].w = M(3, 3) + M(3, 1);

		//
		// Top
		//
		planes[3].x = M(0, 3) - M(0, 1);
		planes[3].y = M(1, 3) - M(1, 1);
		planes[3].z = M(2, 3) - M(2, 1);
		planes[3].w = M(3, 3) - M(3, 1);

		//
		// Near
		//
		planes[4].x = M(0, 2);
		planes[4].y = M(1, 2);
		planes[4].z = M(2, 2);
		planes[4].w = M(3, 2);

		//
		// Far
		//
		planes[5].x = M(0, 3) - M(0, 2);
		planes[5].y = M(1, 3) - M(1, 2);
		planes[5].z = M(2, 3) - M(2, 2);
		planes[5].w = M(3, 3) - M(3, 2);

		// Normalize the plane equations.
		for (int i = 0; i < 6; ++i)
		{
			XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&planes[i]));
			XMStoreFloat4(&planes[i], v);
		}
	}

	// A faster version of memcopy that uses SSE instructions.  TODO:  Write an ARM variant if necessary.
	void SIMDMemoryCopy(void* __restrict _Dest, const void* __restrict _Source, size_t NumQuadwords)
	{
		assert(GlareEngine::Math::IsAligned(_Dest, 16));
		assert(GlareEngine::Math::IsAligned(_Source, 16));

		__m128i* __restrict Dest = (__m128i * __restrict)_Dest;
		const __m128i* __restrict Source = (const __m128i * __restrict)_Source;

		// Discover how many quadwords precede a cache line boundary.  Copy them separately.
		size_t InitialQuadwordCount = (4 - ((size_t)Source >> 4) & 3) & 3;
		if (InitialQuadwordCount > NumQuadwords)
			InitialQuadwordCount = NumQuadwords;

		switch (InitialQuadwordCount)
		{
		case 3: _mm_stream_si128(Dest + 2, _mm_load_si128(Source + 2));     // Fall through
		case 2: _mm_stream_si128(Dest + 1, _mm_load_si128(Source + 1));     // Fall through
		case 1: _mm_stream_si128(Dest + 0, _mm_load_si128(Source + 0));     // Fall through
		default:
			break;
		}

		if (NumQuadwords == InitialQuadwordCount)
			return;

		Dest += InitialQuadwordCount;
		Source += InitialQuadwordCount;
		NumQuadwords -= InitialQuadwordCount;

		size_t CacheLines = NumQuadwords >> 2;

		switch (CacheLines)
		{
		default:
		case 10: _mm_prefetch((char*)(Source + 36), _MM_HINT_NTA);    // Fall through
		case 9:  _mm_prefetch((char*)(Source + 32), _MM_HINT_NTA);    // Fall through
		case 8:  _mm_prefetch((char*)(Source + 28), _MM_HINT_NTA);    // Fall through
		case 7:  _mm_prefetch((char*)(Source + 24), _MM_HINT_NTA);    // Fall through
		case 6:  _mm_prefetch((char*)(Source + 20), _MM_HINT_NTA);    // Fall through
		case 5:  _mm_prefetch((char*)(Source + 16), _MM_HINT_NTA);    // Fall through
		case 4:  _mm_prefetch((char*)(Source + 12), _MM_HINT_NTA);    // Fall through
		case 3:  _mm_prefetch((char*)(Source + 8), _MM_HINT_NTA);    // Fall through
		case 2:  _mm_prefetch((char*)(Source + 4), _MM_HINT_NTA);    // Fall through
		case 1:  _mm_prefetch((char*)(Source + 0), _MM_HINT_NTA);    // Fall through

			// Do four quadwords per loop to minimize stalls.
			for (size_t i = CacheLines; i > 0; --i)
			{
				// If this is a large copy, start prefetching future cache lines.  This also prefetches the
				// trailing quadwords that are not part of a whole cache line.
				if (i >= 10)
					_mm_prefetch((char*)(Source + 40), _MM_HINT_NTA);

				_mm_stream_si128(Dest + 0, _mm_load_si128(Source + 0));
				_mm_stream_si128(Dest + 1, _mm_load_si128(Source + 1));
				_mm_stream_si128(Dest + 2, _mm_load_si128(Source + 2));
				_mm_stream_si128(Dest + 3, _mm_load_si128(Source + 3));

				Dest += 4;
				Source += 4;
			}

		case 0:    // No whole cache lines to read
			break;
		}

		// Copy the remaining quadwords
		switch (NumQuadwords & 3)
		{
		case 3: _mm_stream_si128(Dest + 2, _mm_load_si128(Source + 2));     // Fall through
		case 2: _mm_stream_si128(Dest + 1, _mm_load_si128(Source + 1));     // Fall through
		case 1: _mm_stream_si128(Dest + 0, _mm_load_si128(Source + 0));     // Fall through
		default:
			break;
		}

		_mm_sfence();
	}


	void SIMDMemoryFill(void* __restrict _Dest, __m128 FillVector, size_t NumQuadwords)
	{
		assert(GlareEngine::Math::IsAligned(_Dest, 16));

		const __m128i Source = _mm_castps_si128(FillVector);
		__m128i* __restrict Dest = (__m128i * __restrict)_Dest;

		switch (((size_t)Dest >> 4) & 3)
		{
		case 1: _mm_stream_si128(Dest++, Source); --NumQuadwords;     // Fall through
		case 2: _mm_stream_si128(Dest++, Source); --NumQuadwords;     // Fall through
		case 3: _mm_stream_si128(Dest++, Source); --NumQuadwords;     // Fall through
		default:
			break;
		}

		size_t WholeCacheLines = NumQuadwords >> 2;

		// Do four quadwords per loop to minimize stalls.
		while (WholeCacheLines--)
		{
			_mm_stream_si128(Dest++, Source);
			_mm_stream_si128(Dest++, Source);
			_mm_stream_si128(Dest++, Source);
			_mm_stream_si128(Dest++, Source);
		}

		// Copy the remaining quadwords
		switch (NumQuadwords & 3)
		{
		case 3: _mm_stream_si128(Dest++, Source);     // Fall through
		case 2: _mm_stream_si128(Dest++, Source);     // Fall through
		case 1: _mm_stream_si128(Dest++, Source);     // Fall through
		default:
			break;
		}

		_mm_sfence();
	}

	std::wstring StringToWString(const std::string& str)
	{
		return std::wstring(str.begin(), str.end());
	}


	std::string WStringToString(const std::wstring& str)
	{

		_bstr_t t = str.c_str();
		char* pchar = (char*)t;
		string result = pchar;
		return result;
	}

	bool CheckFileExist(const std::wstring& FileName)
	{
		ifstream f(FileName);
		if (!f.good())
		{
#ifdef _DEBUG
			wstring Message = L"File ";
			Message += FileName.c_str();
			Message += L" do not exist!\n";
			EngineLog::AddLog(Message.c_str());
			::OutputDebugString(Message.c_str());
#endif // DEBUG
			return false;
		}
		return true;
	}

	std::string GetBasePath(const std::string& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind('/')) != std::string::npos)
			return filePath.substr(0, lastSlash + 1);
		else if ((lastSlash = filePath.rfind('\\')) != std::string::npos)
			return filePath.substr(0, lastSlash + 1);
		else
			return "";
	}

	std::wstring GetBasePath(const std::wstring& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind(L'/')) != std::wstring::npos)
			return filePath.substr(0, lastSlash + 1);
		else if ((lastSlash = filePath.rfind(L'\\')) != std::wstring::npos)
			return filePath.substr(0, lastSlash + 1);
		else
			return L"";
	}

	std::string RemoveBasePath(const std::string& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind('/')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else if ((lastSlash = filePath.rfind('\\')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else
			return filePath;
	}

	std::wstring RemoveBasePath(const std::wstring& filePath)
	{
		size_t lastSlash;
		if ((lastSlash = filePath.rfind(L'/')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else if ((lastSlash = filePath.rfind(L'\\')) != std::string::npos)
			return filePath.substr(lastSlash + 1, std::string::npos);
		else
			return filePath;
	}

	std::string GetFileExtension(const std::string& filePath)
	{
		std::string fileName = RemoveBasePath(filePath);
		size_t extOffset = fileName.rfind('.');
		if (extOffset == std::wstring::npos)
			return "";

		return fileName.substr(extOffset + 1);
	}

	std::wstring GetFileExtension(const std::wstring& filePath)
	{
		std::wstring fileName = RemoveBasePath(filePath);
		size_t extOffset = fileName.rfind(L'.');
		if (extOffset == std::wstring::npos)
			return L"";

		return fileName.substr(extOffset + 1);
	}

	std::string RemoveExtension(const std::string& filePath)
	{
		return filePath.substr(0, filePath.rfind("."));
	}

	std::wstring RemoveExtension(const std::wstring& filePath)
	{
		return filePath.substr(0, filePath.rfind(L"."));
	}


	std::string ToLower(const std::string& str)
	{
		std::string lower_case = str;
		std::locale loc;
		for (char& s : lower_case)
			s = std::tolower(s, loc);
		return lower_case;
	}

	std::wstring ToLower(const std::wstring& str)
	{
		std::wstring lower_case = str;
		std::locale loc;
		for (wchar_t& s : lower_case)
			s = std::tolower(s, loc);
		return lower_case;
	}

	/// Throw an error if we encounter program crashing error in Windows.
	/// Will display an error message box and trow the calling process.
	///
	/// @param [in] format  The formatted string to parse.
	///
	/// @returns            None.
	inline void GlareCritical(const wchar_t* format, ...)
	{
		// Format the message string
		wchar_t buffer[512];

		va_list args;
		va_start(args, format);
		vswprintf(buffer, 512, format, args);
		va_end(args);

		EngineLog::AddLog(buffer);
		MessageBoxW(NULL, buffer, L"Critical Error", MB_OK);
		throw 1;
	}

	void GlareError(const wchar_t* format, ...)
	{
		// Format the message string
		wchar_t buffer[512];

		va_list args;
		va_start(args, format);
		vswprintf(buffer, 512, format, args);
		va_end(args);

		EngineLog::AddLog(buffer);
#if defined(_DEBUG)
		MessageBoxW(NULL, buffer, L"Error", MB_OK);
#endif // #if defined(_DEBUG)
	}

	void GlareWarning(const wchar_t* format, ...)
	{
		// Format the message string
		wchar_t buffer[1024];

		va_list args;
		va_start(args, format);
		vswprintf(buffer, 1024, format, args);
		va_end(args);

		EngineLog::AddLog(buffer);
	}

	void GlareAssert(AssertLevel severity, bool condition, const wchar_t* format, ...)
	{
		if (!condition)
		{
			wchar_t buffer[512];

			va_list args;
			va_start(args, format);
			vswprintf(buffer, 512, format, args);
			va_end(args);

			// Most common
			if (severity == ASSERT_CRITICAL)
				GlareCritical(buffer);

			else if (severity == ASSERT_ERROR)
				GlareError(buffer);

			else
				GlareWarning(buffer);
		}
	}
}