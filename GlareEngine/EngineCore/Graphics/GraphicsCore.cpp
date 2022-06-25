#include "GraphicsCore.h"
#include "SamplerManager.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "RootSignature.h"
#include "CommandSignature.h"
#include "EngineAdjust.h"
#include "ColorBuffer.h"
#include "GPUTimeManager.h"
#include "EngineLog.h"
#include "BufferManager.h"
#include "TextureManager.h"
#include "Display.h"

#include "ScreenGrab.h"
//Shaders
#include "CompiledShaders/PresentSDRPS.h"
#include "CompiledShaders/ScreenQuadVS.h"
#include "CompiledShaders/BufferCopyPS.h"

// 该宏决定是否检测是否有 HDR 显示并启用 HDR10 输出。
// 目前，在启用 HDR 显示的情况下，像素放大功能被破坏。
#define CONDITIONALLY_ENABLE_HDR_OUTPUT 1

#define SWAP_CHAIN_BUFFER_COUNT 3


#if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
#include <dxgi1_6.h>
#else
#include <dxgi1_4.h>    // For WARP
#endif
#include <winreg.h>        // To read the registry

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

namespace GlareEngine
{
	using namespace GlareEngine::Math;
	using namespace GlareEngine::Display;

	//HDR FORMAT
	bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT = false;
	bool g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = false;

	bool g_bUseGPUBasedValidation = false;
	bool g_bUseWarpDriver = false;
	//Device
	ID3D12Device* g_Device = nullptr;
	CommandListManager g_CommandManager;
	ContextManager g_ContextManager;

	//FEATURE LEVEL 11
	D3D_FEATURE_LEVEL g_D3DFeatureLevel = D3D_FEATURE_LEVEL_11_0;

	//Descriptor Allocator
	DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
	{
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
	};

	static const uint32_t vendorID_NVIDIA = 0x10DE;
	static const uint32_t vendorID_AMD = 0x1002;
	static const uint32_t vendorID_Intel = 0x8086;

	//SRV Descriptors Manager ,return Descriptor index
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_TextureSRV;
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> g_CubeSRV;



	// Check adapter support for DirectX Raytracing.
	bool IsDirectXRaytracingSupported(ID3D12Device* testDevice)
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupport = {};

		if (FAILED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupport, sizeof(featureSupport))))
			return false;

		return featureSupport.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	}

	uint32_t GetVendorIdFromDevice(ID3D12Device* pDevice)
	{
		LUID luid = pDevice->GetAdapterLuid();

		// Obtain the DXGI factory
		Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
		SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)));

		Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;

		if (SUCCEEDED(dxgiFactory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&pAdapter))))
		{
			DXGI_ADAPTER_DESC1 desc;
			if (SUCCEEDED(pAdapter->GetDesc1(&desc)))
			{
				return desc.VendorId;
			}
		}
		return 0;
	}

	bool IsDeviceNVIDIA(ID3D12Device* pDevice)
	{
		return GetVendorIdFromDevice(pDevice) == vendorID_NVIDIA;
	}

	bool IsDeviceAMD(ID3D12Device* pDevice)
	{
		return GetVendorIdFromDevice(pDevice) == vendorID_AMD;
	}

	bool IsDeviceIntel(ID3D12Device* pDevice)
	{
		return GetVendorIdFromDevice(pDevice) == vendorID_Intel;
	}


	void InitializeGraphics(bool RequireDXRSupport)
	{
		Microsoft::WRL::ComPtr<ID3D12Device> pDevice;

		uint32_t useDebugLayers = 0;
#if _DEBUG
		// Default to true for debug builds
		useDebugLayers = 1;
#endif

		DWORD dxgiFactoryFlags = 0;

		if (useDebugLayers)
		{
			Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
			{
				debugInterface->EnableDebugLayer();

				if (g_bUseGPUBasedValidation)
				{
					Microsoft::WRL::ComPtr<ID3D12Debug1> debugInterface1;
					if (SUCCEEDED((debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1)))))
					{
						debugInterface1->SetEnableGPUBasedValidation(true);
					}
				}
			}
			else
			{
				EngineLog::AddLog(L"WARNING:  Unable to enable D3D12 debug validation layer\n");
			}

#if _DEBUG
			Microsoft::WRL::ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
			if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
			{
				dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

				DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
				{
					80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
				};
				DXGI_INFO_QUEUE_FILTER filter = {};
				filter.DenyList.NumIDs = _countof(hide);
				filter.DenyList.pIDList = hide;
				dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
			}
#endif
		}
		// Obtain the DXGI factory
		Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
		SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

		// Create the D3D graphics device
		Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;

		// Temporary workaround because SetStablePowerState() is crashing
		D3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr);

		if (!g_bUseWarpDriver)
		{
			SIZE_T MaxSize = 0;

			for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
			{
				DXGI_ADAPTER_DESC1 desc;
				pAdapter->GetDesc1(&desc);

				// Is a software adapter?
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					continue;

				// Can create a D3D12 device?
				if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&pDevice))))
					continue;

				// Does support DXR if required?
				if (RequireDXRSupport && !IsDirectXRaytracingSupported(pDevice.Get()))
					continue;

				// By default, search for the adapter with the most memory because that's usually the dGPU.
				if (desc.DedicatedVideoMemory < MaxSize)
					continue;

				MaxSize = desc.DedicatedVideoMemory;

				if (g_Device != nullptr)
					g_Device->Release();

				g_Device = pDevice.Detach();

				EngineLog::AddLog(L"Selected GPU:  %s (%u MB)", desc.Description, desc.DedicatedVideoMemory >> 20);
			}
		}

		if (RequireDXRSupport && !g_Device)
		{
			EngineLog::AddLog(L"Unable to find a DXR-capable device. Halting.\n");
			__debugbreak();
		}

		if (g_Device == nullptr)
		{
			if (g_bUseWarpDriver)
				EngineLog::AddLog(L"WARP software adapter requested.  Initializing...\n");
			else
				EngineLog::AddLog(L"Failed to find a hardware adapter.Falling back to WARP.\n");
			SUCCEEDED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter)));
			SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));
			g_Device = pDevice.Detach();
		}
#ifndef RELEASE
		else
		{
			bool DeveloperModeEnabled = false;

			// Look in the Windows Registry to determine if Developer Mode is enabled
			HKEY hKey;
			LSTATUS result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hKey);
			if (result == ERROR_SUCCESS)
			{
				DWORD keyValue, keySize = sizeof(DWORD);
				result = RegQueryValueEx(hKey, L"AllowDevelopmentWithoutDevLicense", 0, NULL, (byte*)&keyValue, &keySize);
				if (result == ERROR_SUCCESS && keyValue == 1)
					DeveloperModeEnabled = true;
				RegCloseKey(hKey);
			}
			// Prevent the GPU from overclocking or underclocking to get consistent timings
			if (DeveloperModeEnabled)
				g_Device->SetStablePowerState(TRUE);
		}
#endif	
#if _DEBUG
		ID3D12InfoQueue* pInfoQueue = nullptr;
		if (SUCCEEDED(g_Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
		{
			// Suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID DenyIds[] =
			{
				// This occurs when there are uninitialized descriptors in a descriptor table, even when a
				// shader does not access the missing descriptors.  I find this is common when switching
				// shader permutations and not wanting to change much code to reorder resources.
				D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,

				// Triggered when a shader does not export all color components of a render target, such as
				// when only writing RGB to an R10G10B10A2 buffer, ignoring alpha.
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,

				// This occurs when a descriptor table is unbound even when a shader does not access the missing
				// descriptors.  This is common with a root signature shared between disparate shaders that
				// don't all need the same types of resources.
				D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,

				// RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS
				(D3D12_MESSAGE_ID)1008,
			};

			D3D12_INFO_QUEUE_FILTER NewFilter = {};
			//NewFilter.DenyList.NumCategories = _countof(Categories);
			//NewFilter.DenyList.pCategoryList = Categories;
			NewFilter.DenyList.NumSeverities = _countof(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			NewFilter.DenyList.NumIDs = _countof(DenyIds);
			NewFilter.DenyList.pIDList = DenyIds;

			pInfoQueue->PushStorageFilter(&NewFilter);
			pInfoQueue->Release();
		}
#endif

		// We like to do read-modify-write operations on UAVs during post processing.  To support that, we
		// need to either have the hardware do typed UAV loads of R11G11B10_FLOAT or we need to manually
		// decode an R32_UINT representation of the same buffer.  This code determines if we get the hardware
		// load support.
		D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData = {};
		if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &FeatureData, sizeof(FeatureData))))
		{
			if (FeatureData.TypedUAVLoadAdditionalFormats)
			{
				D3D12_FEATURE_DATA_FORMAT_SUPPORT Support =
				{
					DXGI_FORMAT_R11G11B10_FLOAT, D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE
				};

				if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
					(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
				{
					g_bTypedUAVLoadSupport_R11G11B10_FLOAT = true;
				}

				Support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

				if (SUCCEEDED(g_Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
					(Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
				{
					g_bTypedUAVLoadSupport_R16G16B16A16_FLOAT = true;
				}
			}
		}

		g_CommandManager.Create(g_Device);

		// Common state was moved to GraphicsCommon.
		InitializeAllCommonState();

		Display::Initialize();

		GPUTimeManager::Initialize(4096);

		/*TemporalEffects::Initialize();
		PostEffects::Initialize();
		SSAO::Initialize();
		ParticleEffectManager::Initialize(3840, 2160);*/
	}

	void ShutdownGraphics(void)
	{
		g_CommandManager.IdleGPU();
		CommandContext::DestroyAllContexts();
		g_CommandManager.Shutdown();
		GPUTimeManager::Shutdown();
		PSO::DestroyAll();
		RootSignature::DestroyAll();
		DescriptorAllocator::DestroyAll();

		DestroyCommonState();
		DestroyRenderingBuffers();

		//TemporalEffects::Shutdown();
		//PostEffects::Shutdown();
		//SSAO::Shutdown();
		//ParticleEffects::Shutdown();

		Display::Shutdown();

#if defined(_DEBUG)
		ID3D12DebugDevice* debugInterface;
		if (SUCCEEDED(g_Device->QueryInterface(&debugInterface)))
		{
			//debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
			debugInterface->Release();
		}
#endif
		SAFE_RELEASE(g_Device);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count)
	{
		return g_DescriptorAllocator[Type].Allocate(Count);
	}

	int AddToGlobalTextureSRVDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes)
	{
		g_TextureSRV.push_back(SRVdes);
		return int(g_TextureSRV.size() - 1);
	}
	int AddToGlobalCubeSRVDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& SRVdes)
	{
		g_CubeSRV.push_back(SRVdes);
		return int(g_CubeSRV.size() - 1);
	}
}



