#pragma once

// ���� SDKDDKVer.h �ɶ�����õ���߰汾�� Windows ƽ̨��
// ���ϣ��Ϊ֮ǰ�� Windows ƽ̨����Ӧ�ó����ڰ��� SDKDDKVer.h ֮ǰ���Ȱ��� WinSDKVer.h ��
// �� _WIN32_WINNT ������Ϊ��Ҫ֧�ֵ�ƽ̨��
#include <SDKDDKVer.h>

// �� Windows ͷ�ļ����ų�����ʹ�õ�����
#define WIN32_LEAN_AND_MEAN             
#define STB_IMAGE_IMPLEMENTATION

// C ����ʱͷ�ļ�
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <windows.h>
#include <wrl.h>
#include <string>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include <map>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <ppltasks.h>

#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "MathHelper.h"
#include "GameTimer.h"
#include "VectorMath.h"
#include "Color.h"
#include "EngineAdjust.h"
#include "Camera.h"
#include "EngineGlobal.h"

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

using namespace Microsoft::WRL;
using namespace DirectX;

//config
#define REVERSE_Z 1


extern const int gNumFrameResources;

inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
{
    if (obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
{
    if (obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
    if (obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class EngineUtility
{
public:

    static bool IsKeyDown(int vkeyCode);

    static std::string ToString(HRESULT hr) = delete;

    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateRandomTexture1DSRV(ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);



    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);


    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefault2DTexture(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmdList,
        const void* initData,
        UINT64 byteSize,
        DXGI_FORMAT format,
        UINT64 width,
        UINT height,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);


    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* defines,
        const std::string& entrypoint,
        const std::string& target);


    static void CreateWICTextureFromFile(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, ID3D12Resource** tex, ID3D12Resource** Uploadtex, std::wstring filename);
    static void CreateWICTextureFromMemory(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* CommandList, ID3D12Resource** tex, ID3D12Resource** Uploadtex, unsigned char* data, int size);
};

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};



struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot light only
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
    float FalloffEnd = 10.0f;                           // point/spot light only
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
    float SpotPower = 64.0f;                            // spot light only
};

#define MaxLights 16


// Order: left, right, bottom, top, near, far.
void ExtractFrustumPlanes(XMFLOAT4 planes[6], CXMMATRIX M);


// ------------------------------
// DXTraceW����
// ------------------------------
// �ڵ�����������������ʽ��������Ϣ����ѡ�Ĵ��󴰿ڵ���(�Ѻ���)
// [In]strFile			��ǰ�ļ�����ͨ�����ݺ�__FILEW__
// [In]hlslFileName     ��ǰ�кţ�ͨ�����ݺ�__LINE__
// [In]hr				����ִ�г�������ʱ���ص�HRESULTֵ
// [In]strMsg			���ڰ������Զ�λ���ַ�����ͨ������L#x(����ΪNULL)
// [In]bPopMsgBox       ���ΪTRUE���򵯳�һ����Ϣ������֪������Ϣ
// ����ֵ: �β�hr
HRESULT WINAPI DXTraceW(_In_z_ const WCHAR* strFile, _In_ DWORD dwLine, _In_ HRESULT hr, _In_opt_ const WCHAR* strMsg, _In_ bool bPopMsgBox);


// ------------------------------
// ThrowIfFailed��
// ------------------------------
// Debugģʽ�µĴ���������׷��
#if defined(DEBUG) | defined(_DEBUG)
#ifndef ThrowIfFailed
#define ThrowIfFailed(x)												\
	{																	\
		HRESULT hr__ = (x);												\
		if(FAILED(hr__))												\
		{																\
			DXTraceW(__FILEW__, (DWORD)__LINE__, hr__, L#x, true);		\
		}																\
	}
#endif
#else
#ifndef ThrowIfFailed
#define ThrowIfFailed(x) (x)
#endif 
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif


void SIMDMemoryCopy(void* __restrict Dest, const void* __restrict Source, size_t NumQuadwords);
void SIMDMemoryFill(void* __restrict Dest, __m128 FillVector, size_t NumQuadwords);

std::wstring StringToWString(const std::string& str);
std::string WStringToString(const std::wstring& wstr);
std::string WstringToUTF8(const std::wstring& str);
std::wstring UTF8ToWstring(const std::string& str);

bool CheckFileExist(const std::wstring& FileName);

std::string GetBasePath(const std::string& str);
std::wstring GetBasePath(const std::wstring& str);
std::string RemoveBasePath(const std::string& str);
std::wstring RemoveBasePath(const std::wstring& str);
std::string GetFileExtension(const std::string& str);
std::wstring GetFileExtension(const std::wstring& str);
std::string RemoveExtension(const std::string& str);
std::wstring RemoveExtension(const std::wstring& str);
std::string ToLower(const std::string& str);
std::wstring ToLower(const std::wstring& str);