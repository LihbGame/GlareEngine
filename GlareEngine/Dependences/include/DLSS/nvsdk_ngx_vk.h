/*
 * SPDX-FileCopyrightText: Copyright (c) 2019-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

/*
*  HOW TO USE:
*
*  IMPORTANT: Methods in this library are NOT thread safe. It is up to the
*  client to ensure that thread safety is enforced as needed.
*  
*  1) Call NVSDK_CONV NVSDK_NGX_D3D11/D3D12/CUDA_Init and pass your app Id
*     and other parameters. This will initialize SDK or return an error code
*     if SDK cannot run on target machine. Depending on error user might
*     need to update drivers. Please note that application Id is provided
*     by NVIDIA so if you do not have one please contact us.
* 
*  2) Call NVSDK_NGX_D3D11/D3D12/CUDA_GetParameters to obtain pointer to 
*     interface used to pass parameters to SDK. Interface instance is 
*     allocated and released by SDK so there is no need to do any memory 
*     management on client side.
*    
*  3) Set key parameters for the feature you want to use. For example, 
*     width and height are required for all features and they can be
*     set like this: 
*         Params->Set(NVSDK_NGX_Parameter_Width,MY_WIDTH);
*         Params->Set(NVSDK_NGX_Parameter_Height,MY_HEIGHT);
*
*     You can also provide hints like NVSDK_NGX_Parameter_Hint_HDR to tell
*     SDK that it should expect HDR color space is needed. Please refer to 
*     samples since different features need different parameters and hints.
*
*  4) Call NVSDK_NGX_D3D11/D3D12/CUDA_GetScratchBufferSize to obtain size of
*     the scratch buffer needed by specific feature. This D3D or CUDA buffer
*     should be allocated by client and passed as:
*        Params->Set(NVSDK_NGX_Parameter_Scratch,MY_SCRATCH_POINTER)
*        Params->Set(NVSDK_NGX_Parameter_Scratch_SizeInBytes,MY_SCRATCH_SIZE_IN_BYTES)
*     NOTE: Returned size can be 0 if feature does not use any scratch buffer.
*     It is OK to use bigger buffer or reuse buffers across features as long
*     as minimum size requirement is met.
*
*  5) Call NVSDK_NGX_D3D11/D3D12/CUDA_CreateFeature to create feature you need.
*     On success SDK will return a handle which must be used in any successive 
*     calls to SDK which require feature handle. SDK will use all parameters
*     and hints provided by client to generate feature. If feature with the same
*     parameters already exists and error code will be returned.
*
*  6) Call NVSDK_NGX_D3D11/D3D12/CUDA_EvaluateFeature to invoke execution of
*     specific feature. Before feature can be evaluated input parameters must
*     be specified (like for example color/albedo buffer, motion vectors etc)
* 
*  6) Call NVSDK_NGX_D3D11/D3D12/CUDA_ReleaseFeature when feature is no longer
*     needed. After this call feature handle becomes invalid and cannot be used.
* 
*  7) Call NVSDK_NGX_D3D11/D3D12/CUDA_Shutdown when SDK is no longer needed to
*     release all resources.

*  Contact: ngxsupport@nvidia.com
*/


#ifndef NVSDK_NGX_VK_H
#define NVSDK_NGX_VK_H

#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_params.h"
#ifndef __cplusplus
#include <stdbool.h> 
#include <wchar.h> 
#endif

#ifdef __cplusplus
extern "C"
{
#endif

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_ImageViewInfo_VK [Vulkan only]
// Contains ImageView-specific metadata.
// ImageView:
//    The VkImageView resource.
//
// Image:
//    The VkImage associated to this VkImageView.
//
// SubresourceRange:
//    The VkImageSubresourceRange associated to this VkImageView.
//
//  Format:
//    The format of the resource.
//
//  Width:
//     The width of the resource.
// 
//  Height:
//     The height of the resource.
//
typedef struct NVSDK_NGX_ImageViewInfo_VK {
    VkImageView ImageView;
    VkImage Image;
    VkImageSubresourceRange SubresourceRange;
    VkFormat Format;
    unsigned int Width;
    unsigned int Height;
} NVSDK_NGX_ImageViewInfo_VK;

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_BufferInfo_VK [Vulkan only]
// Contains Buffer-specific metadata.
// Buffer
//    The VkBuffer resource.
//
// SizeInBytes:
//    The size of the resource (in bytes).
//
typedef struct NVSDK_NGX_BufferInfo_VK {
    VkBuffer Buffer;
    unsigned int SizeInBytes;
} NVSDK_NGX_BufferInfo_VK;

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Resource_VK [Vulkan only]
//
// ImageViewInfo:
//    The VkImageView resource, and VkImageView-specific metadata. A NVSDK_NGX_Resource_VK can only have one of ImageViewInfo or BufferInfo.
//
// BufferInfo:
//    The VkBuffer Resource, and VkBuffer-specific metadata. A NVSDK_NGX_Resource_VK can only have one of ImageViewInfo or BufferInfo.
// 
//  Type:
//    Whether or this resource is a VkImageView or a VkBuffer.
// 
//  ReadWrite:
//     True if the resource is available for read and write access.
//          For VkBuffer resources: VkBufferUsageFlags includes VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT or VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
//          For VkImage resources: VkImageUsageFlags for associated VkImage includes VK_IMAGE_USAGE_STORAGE_BIT
//
typedef struct NVSDK_NGX_Resource_VK {
    union {
        NVSDK_NGX_ImageViewInfo_VK ImageViewInfo;
        NVSDK_NGX_BufferInfo_VK BufferInfo;
    } Resource;
    NVSDK_NGX_Resource_VK_Type Type;
    bool ReadWrite;
} NVSDK_NGX_Resource_VK;

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_VULKAN_RequiredExtensions [Vulkan only]
// -------------------------------------
// DEPRECATED: Use NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements() and 
// NVSDK_NGX_VULKAN_GetFeatureRequirements() instead.
//
// Retrieves the instance and device extensions required by NGX.
//
// DESCRIPTION:
//      This function returns the instance and device extensions required by
//      NGX. It is deprecated and should be replaced with
//      NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements() and
//      NVSDK_NGX_VULKAN_GetFeatureRequirements().
//
// PARAMETERS:
// OutInstanceExtCount:
//      Returns the number of instance extensions NGX requires.
//
// OutInstanceExts:
//      Returns a pointer to *OutInstanceExtCount strings of instance extensions.
//
// OutDeviceExtCount:
//      Returns the number of device extensions NGX requires.
//
// OutDeviceExts:
//      Returns a pointer to *OutDeviceExtCount strings of device extensions.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_RequiredExtensions(unsigned int *OutInstanceExtCount, const char ***OutInstanceExts, unsigned int *OutDeviceExtCount, const char ***OutDeviceExts);

// NOTE: Functions under the same name and different function signatures exist
// between the NGX SDK, NGX Core (driver), and NGX Snippets. To discern the
// different signatures here we check if NGX_SNIPPET_BUILD is defined. When that
// is the case we know that the function signature in-use should be that of
// between NGX Core and the Snippet. Otherwise the signature should be between
// NGX SDK and NGX Core.
////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Init
// -----------------------------------------------------------------------------
// Initializes a new SDK instance.
//
// DESCRIPTION:
//      Initializes a new SDK instance. Most NGX SDK functions require the NGX
//      SDK to be initialized for the corresponding graphics API and device to
//      function properly.
//
//      NVSDK_NGX_Init requires an application ID provided by NVIDIA. If an
//      application ID is not available, use NVSDK_NGX_Init_with_ProjectID to
//      supply your own identifier.
//
//      Be sure to call NVSDK_NGX_Shutdown to shut down the SDK when it is no
//      longer needed.
//
// PARAMETERS:
// InApplicationId:
//      Unique ID provided by NVIDIA.
//
// InApplicationDataPath:
//      Directory to store logs and other temporary files. Write access is
//      required. Typically, this would be next to the application executable,
//      or in a location like Documents or ProgramData.
//
// InInstance/InPD/InDevice:
//      Vulkan Instance, PhysicalDevice, and Device to use.
//
// InGIPA/InGDPA:
//      Optional Vulkan function pointers to vkGetInstanceProcAddr and
//      vkGetDeviceProcAddr.
//
// InFeatureInfo:
//      Contains information common to all features, including a list of paths
//      where feature DLLs can be located, other than the default path (application directory).
//
// InSDKVersion:
//      Version of the SDK currently in use. Typically, this should be left as
//      the default value, NVSDK_NGX_Version_API.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure. Common values include:
//        - NVSDK_NGX_Result_Success if the function executed successfully
//        - NVSDK_NGX_Result_FAIL_FeatureNotSupported if the NGX SDK is not
//          supported on the current system
//      Check the NGX logs for additional information about any failures.
//
#if defined(NGX_SNIPPET_BUILD)
#ifdef __cplusplus
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, const NVSDK_NGX_Parameter *InParameters = nullptr);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init_Ext2(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA = nullptr, PFN_vkGetDeviceProcAddr InGDPA = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API, const NVSDK_NGX_Parameter *InParameters = nullptr);
#else
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init_Ext(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_Parameter *InParameters);
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init_Ext2(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA, NVSDK_NGX_Version InSDKVersion, const NVSDK_NGX_Parameter *InParameters);
#endif
#else
#ifdef __cplusplus
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA = nullptr, PFN_vkGetDeviceProcAddr InGDPA = nullptr, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
#else
NVSDK_NGX_API NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
#endif
#endif // defined(NGX_SNIPPET_BUILD)

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Init_with_ProjectID
// -----------------------------------------------------------------------------
// Initializes a new SDK instance using a project ID.
//
// DESCRIPTION:
//      Initializes a new SDK instance. Most NGX SDK functions require the NGX
//      SDK to be initialized for the corresponding graphics API and device to
//      function properly.
//
//      Applications must provide a unique identifier to initialize the NGX SDK.
//      If an identifier was provided by NVIDIA, use NVSDK_NGX_Init instead.
//
//      For projects using third-party engines such as Unreal Engine or
//      Omniverse, the integration for those engines should handle providing the
//      correct Project ID to NGX. Ensure that the project ID is set in the
//      engine's editor.
//
//      For other engines, use NVSDK_NGX_EngineType CUSTOM. The driver will
//      validate that the Project ID is GUID-like, for example,
//      "a0f57b54-1daf-4934-90ae-c4035c19df04". Ensure your project ID matches
//      this format.
//
//      Be sure to call NVSDK_NGX_Shutdown to shut down the SDK when it is no
//      longer needed.
//
// PARAMETERS:
// InProjectId:
//      Unique ID provided by the rendering engine used. Must be a GUID-like
//      string.
//
// InEngineType:
//      Rendering engine used by the application/plugin. Use NVSDK_NGX_EngineType
//      CUSTOM if the specific engine type is not explicitly supported.
//
// InEngineVersion:
//      Version number of the rendering engine used by the application/plugin.
//
// InApplicationDataPath:
//      Directory to store logs and other temporary files (write access required).
//
// InInstance/InPD/InDevice:
//      Vulkan Instance, PhysicalDevice, and Device to use.
//
// InGIPA/InGDPA:
//      Optional Vulkan function pointers to vkGetInstanceProcAddr and
//      vkGetDeviceProcAddr.
//
// InFeatureInfo:
//      Contains information common to all features, including a list of paths
//      where feature DLLs can be located in addition to the default path
//      (the application directory).
//
// InSDKVersion:
//      Version of the SDK currently in use. Typically, this should be left as
//      the default value, NVSDK_NGX_Version_API.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure. Common values include:
//        - NVSDK_NGX_Result_Success if the function executed successfully
//        - NVSDK_NGX_Result_InvalidParameter if the provided project ID is in
//          an invalid format
//        - NVSDK_NGX_Result_FAIL_FeatureNotSupported if the NGX SDK is not
//          supported on the current system
//      Check the NGX logs for additional information about any failures.
//
#if defined(NGX_SNIPPET_BUILD)
// No NGX Core <---> Snippet interfaces
#else
#ifdef __cplusplus
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA = nullptr, PFN_vkGetDeviceProcAddr InGDPA = nullptr, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo = nullptr, NVSDK_NGX_Version InSDKVersion = NVSDK_NGX_Version_API);
#else
NVSDK_NGX_Result  NVSDK_CONV NVSDK_NGX_VULKAN_Init_with_ProjectID(const char *InProjectId, NVSDK_NGX_EngineType InEngineType, const char *InEngineVersion, const wchar_t *InApplicationDataPath, VkInstance InInstance, VkPhysicalDevice InPD, VkDevice InDevice, PFN_vkGetInstanceProcAddr InGIPA, PFN_vkGetDeviceProcAddr InGDPA, const NVSDK_NGX_FeatureCommonInfo *InFeatureInfo, NVSDK_NGX_Version InSDKVersion);
#endif
#endif // defined(NGX_SNIPPET_BUILD)

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_Shutdown
// -----------------------------------------------------------------------------
// Shuts down the current SDK instance and releases all resources.
//
// DESCRIPTION:
//      Shuts down the current SDK instance and releases all resources. If a
//      device is provided, NVSDK_NGX_Shutdown1 shuts down the instance
//      corresponding to the specified device. If nullptr is provided, all
//      instances are shut down.
//
//      Deprecation Notice: The use of Shutdown() is deprecated. Please use
//      Shutdown1(nullptr) instead.
//
// PARAMETERS:
// InDevice:
//      Device to shut down. If nullptr, the SDK is shut down for all devices.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
#ifdef NGX_ENABLE_DEPRECATED_SHUTDOWN
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_Shutdown(void);
#endif
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_Shutdown1(VkDevice InDevice);

#ifdef NGX_ENABLE_DEPRECATED_GET_PARAMETERS
////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetParameters
// -----------------------------------------------------------------------------
// Retrieves a parameter map used to set parameters needed by the SDK.
//
// DEPRECATED: Use NVSDK_NGX_AllocateParameters or
// NVSDK_NGX_GetCapabilityParameters instead.
//
// DESCRIPTION:
//      Retrieves the common NVSDK_NGX_Parameter map for providing parameters to
//      the SDK. The NVSDK_NGX_Parameter interface allows simple parameter setup
//      using named fields. For example, set the width by calling
//      Parameters->Set(NVSDK_NGX_Parameter_Width, 100) or provide a resource
//      pointer by calling Parameters->Set(NVSDK_NGX_Parameter_Color, resource).
//      For more details, see the sample code.
//
//      Parameter maps returned by NVSDK_NGX_GetParameters are pre-populated
//      with NGX capabilities and available features. Unlike
//      NVSDK_NGX_AllocateParameters, parameter maps returned by
//      NVSDK_NGX_GetParameters have their lifetimes managed by NGX and must not
//      be destroyed by the application using NVSDK_NGX_DestroyParameters.
//
//      This function may only be called after a successful call to
//      NVSDK_NGX_Init.
//
//      Note: Allocated memory will be freed by NGX, so do not use the
//      free/delete operator.
//
//      Deprecation Notice: NVSDK_NGX_GetParameters is deprecated. Applications
//      should use NVSDK_NGX_AllocateParameters and
//      NVSDK_NGX_GetCapabilityParameters when possible. However, if there is a
//      possibility that the user may be using an older driver version (driver
//      version 445 or older), NVSDK_NGX_GetParameters may still be used as a
//      fallback if NVSDK_NGX_AllocateParameters or
//      NVSDK_NGX_GetCapabilityParameters return
//      NVSDK_NGX_Result_FAIL_OutOfDate.
//
// PARAMETERS:
// OutParameters:
//      Output pointer that will be populated with an NVSDK_NGX_Parameter
//      interface.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetParameters(NVSDK_NGX_Parameter **OutParameters);
#endif // NGX_ENABLE_DEPRECATED_GET_PARAMETERS

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_AllocateParameters
// -----------------------------------------------------------------------------
// Allocates a parameter map used to set parameters needed by the SDK.
//
// DESCRIPTION:
//      Allocates a new NVSDK_NGX_Parameter map for providing parameters to the
//      SDK. The lifetime of this parameter map must be managed by the
//      application. The NVSDK_NGX_Parameter interface allows simple parameter
//      setup using named fields. For example, set the width by calling
//      Parameters->Set(NVSDK_NGX_Parameter_Width, 100) or provide a resource
//      pointer by calling Parameters->Set(NVSDK_NGX_Parameter_Color, resource).
//      For more details, see the sample code.
//
//      Use NVSDK_NGX_DestroyParameters to free a parameter map created by
//      NVSDK_NGX_AllocateParameters. Parameter maps created by
//      NVSDK_NGX_AllocateParameters must NOT be freed using the free/delete
//      operator.
//
//      Parameter maps created by NVSDK_NGX_AllocateParameters do not come
//      pre-populated with NGX capabilities and available features. To create a
//      new parameter map pre-populated with such information, use
//      NVSDK_NGX_GetCapabilityParameters instead.
//
//      This function may return NVSDK_NGX_Result_FAIL_OutOfDate if using an
//      older driver that does not support this API call. In such a case,
//      NVSDK_NGX_GetParameters may be used as a fallback.
//
//      This function may only be called after a successful call to
//      NVSDK_NGX_Init.
//
// PARAMETERS:
// OutParameters:
//      Output pointer that will be populated with the newly-allocated
//      NVSDK_NGX_Parameter interface.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure. Common values include:
//        - NVSDK_NGX_Result_Success if the function executed successfully
//        - NVSDK_NGX_Result_FAIL_OutOfDate if this function is not supported by
//          the current display driver
//      Check the NGX logs for additional information about any failures.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_AllocateParameters(NVSDK_NGX_Parameter** OutParameters);

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetCapabilityParameters
// -----------------------------------------------------------------------------
// Allocates a parameter map populated with NGX and feature capabilities.
//
// DESCRIPTION:
//      Allocates a new NVSDK_NGX_Parameter map pre-populated with NGX
//      capabilities and information about available features. The output
//      parameter map can also be used in the same ways as a parameter map
//      allocated with NVSDK_NGX_AllocateParameters. However, it is not
//      recommended to use NVSDK_NGX_GetCapabilityParameters unless querying NGX
//      capabilities due to the overhead associated with pre-populating the
//      parameter map.
//
//      Use NVSDK_NGX_DestroyParameters to free a parameter map created by
//      NVSDK_NGX_GetCapabilityParameters. Parameter maps created by
//      NVSDK_NGX_GetCapabilityParameters must NOT be freed using the
//      free/delete operator.
//
//      This function may return NVSDK_NGX_Result_FAIL_OutOfDate if using an
//      older driver that does not support this API call. In such a case,
//      NVSDK_NGX_GetParameters may be used as a fallback.
//
//      This function may only be called after a successful call to
//      NVSDK_NGX_Init.
//
// PARAMETERS:
// OutParameters:
//      Output pointer that will be populated with the newly-allocated
//      NVSDK_NGX_Parameter interface.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure. Common values include:
//        - NVSDK_NGX_Result_Success if the function executed successfully
//        - NVSDK_NGX_Result_FAIL_OutOfDate if this function is not supported by
//          the current display driver
//      Check the NGX logs for additional information about any failures.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetCapabilityParameters(NVSDK_NGX_Parameter** OutParameters);

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_DestroyParameters
// -----------------------------------------------------------------------------
// Destroys the specified parameter map.
//
// DESCRIPTION:
//      Destroys the input parameter map. Once NVSDK_NGX_DestroyParameters is
//      called on a parameter map, it must not be used again.
//
//      NVSDK_NGX_DestroyParameters must not be called on any parameter map
//      returned by NVSDK_NGX_GetParameters; NGX will manage the lifetime of
//      those parameter maps.
//
//      This function may only be called after a successful call to
//      NVSDK_NGX_Init.
//
// PARAMETERS:
// InParameters:
//      The parameter map to be destroyed.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_DestroyParameters(NVSDK_NGX_Parameter* InParameters);

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetScratchBufferSize
// -----------------------------------------------------------------------------
// Retrieves the size of the scratch buffer needed for the specified feature.
//
// DESCRIPTION:
//      The SDK requires a buffer of a certain size provided by the client to
//      initialize some NGX features. Once the feature is no longer needed, the
//      buffer can be released. It is safe to reuse the same scratch buffer for
//      different features as long as the minimum size requirement is met for
//      all features. Some features might not need a scratch buffer, so a return
//      size of 0 is completely valid.
//
//      Note: Most current NGX features do not use scratch buffers. Please check
//      the documentation for the particular feature in use to determine whether
//      a scratch buffer needs to be created.
//
// PARAMETERS:
// InFeatureId:
//      Identifier of the feature to query.
//
// InParameters:
//      Parameters used by the feature to determine scratch buffer size.
//
// OutSizeInBytes:
//      Number of bytes needed for the scratch buffer for the specified feature.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetScratchBufferSize(NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter *InParameters, size_t *OutSizeInBytes);

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_CreateFeature
// -----------------------------------------------------------------------------
// Creates and initializes an instance of the specified NGX feature.
//
// DESCRIPTION:
//      Each feature needs to be created before it can be used. The parameters
//      required to create the feature vary. Refer to the feature's
//      documentation and/or sample code to find out which input parameters are
//      needed to create a specific feature.
//
//      NVSDK_NGX_CreateFeature creates an NVSDK_NGX_Handle to the feature, which
//      is needed to reference the feature in later calls to
//      NVSDK_NGX_EvaluateFeature. Multiple instances of the same feature may be
//      created, and evaluating one instance of a feature should not change the
//      state of other instances.
//
//      When a feature instance is no longer needed, it should be freed using
//      NVSDK_NGX_ReleaseFeature.
//
//      If multiple devices are in use, the device to create the
//      feature on must be specified using CreateFeature1().
//
// PARAMETERS:
// InCmdBuffer:
//      Command buffer to use to execute GPU commands. Must be open and
//      recording.
//
// InDevice:
//      Vulkan device on which to create the feature.
//
// InFeatureID:
//      Identifier of the feature to initialize.
//
// InParameters:
//      Parameters used to create the feature.
//
// OutHandle:
//      Handle which uniquely identifies the feature.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure. Common values include:
//        - NVSDK_NGX_Result_Success if the function executed successfully
//        - NVSDK_NGX_Result_FAIL_UnableToInitializeFeature if the feature
//          cannot be created, for example, because the feature is not supported
//          on the current hardware, or because the DLL could not be found
//        - NVSDK_NGX_Result_FAIL_InvalidParameter if any of the input
//          parameters are invalid
//        - NVSDK_NGX_Result_FAIL_FeatureAlreadyExists if a feature instance
//          with the same parameters has already been created, and the feature
//          doesn't allow multiple instances to be created with the same
//          parameters.
//      Check the NGX logs for additional information about any failures.
//
#if defined(NGX_SNIPPET_BUILD)
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_CreateFeature(VkCommandBuffer InCmdList, NVSDK_NGX_Feature InFeatureID, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_CreateFeature1(VkDevice InDevice, VkCommandBuffer InCmdList, NVSDK_NGX_Feature InFeatureID, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
#else
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_CreateFeature(VkCommandBuffer InCmdBuffer, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_CreateFeature1(VkDevice InDevice, VkCommandBuffer InCmdList, NVSDK_NGX_Feature InFeatureID, NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
#endif // defined(NGX_SNIPPET_BUILD)

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_ReleaseFeature
// -----------------------------------------------------------------------------
// Releases the specified NGX feature.
//
// DESCRIPTION:
//      Releases the feature associated with the given handle and frees its
//      memory. Handles are not reference counted, so after this call, it is
//      invalid to use the provided handle.
//
// PARAMETERS:
// InHandle:
//      Handle to the feature to be released.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_ReleaseFeature(NVSDK_NGX_Handle *InHandle);

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_GetFeatureRequirements
// -----------------------------------------------------------------------------
// Identifies system requirements to support a given NGX feature.
//
// DESCRIPTION:
//      Utility function used to identify system requirements to support a given
//      NGX feature on a system, given its display device subsystem adapter
//      information that will be subsequently used for creating the graphics
//      device. The output parameter OutSupported will be populated with
//      requirements and is valid if and only if the function returns
//      NVSDK_NGX_Result_Success. The result includes:
//        - FeatureSupported: Bitfield of reasons why the feature is
//          unsupported, as specified in NVSDK_NGX_Feature_Support_Result. 0 if
//          the feature is supported
//        - MinHWArchitecture: The hardware architecture version corresponding
//          to an NV_GPU_ARCHITECTURE_ID value defined in the NvAPI GPU
//          Framework
//        - MinOSVersion: The minimum OS version required for the provided
//          feature
//
//      NVSDK_NGX_Init does NOT need to be called before calling this function.
//      Applications may wish to use this function to determine whether a
//      desired feature is supported before initializing the complete SDK.
//
// PARAMETERS:
// Instance:
//      VkInstance in use by the application.
//
// InPhysicalDevice:
//      VkPhysicalDevice in use by the application.
//
// FeatureDiscoveryInfo:
//      Struct containing information about the feature to query and how it is
//      expected to be initialized, including the feature ID and information
//      that would be provided to NVSDK_NGX_Init.
//
// OutSupported:
//      Populated with the requirements for the specified feature.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetFeatureRequirements(const VkInstance Instance,
                                                                                  const VkPhysicalDevice PhysicalDevice,
                                                                                  const NVSDK_NGX_FeatureDiscoveryInfo *FeatureDiscoveryInfo,
                                                                                  NVSDK_NGX_FeatureRequirement *OutSupported);
 
 
////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements
// -----------------------------------------------------------------------------
// Identifies Vulkan instance extensions required for NGX feature support.
//
// DESCRIPTION:
//      This utility function identifies the Vulkan instance extensions required
//      for a specific NGX feature, specified by its feature ID. The function
//      populates OutExtensionCount with the number of extensions required for
//      the NGX feature. It also populates OutExtensionProperties with a pointer
//      to an array of VkExtensionProperties structures, which contains details
//      about each required extension. The size of this array is equal to
//      OutExtensionCount.
//
//      NVSDK_NGX_Init does NOT need to be called before calling this function.
//      This function should be called before the VkInstance for the application
//      is created so that the correct extensions can be enabled.
//
// PARAMETERS:
// FeatureDiscoveryInfo:
//      Struct containing information about the feature to query and how it is
//      expected to be initialized, including the feature ID and information
//      that would be provided to NVSDK_NGX_Init.
//
// OutExtensionCount:
//      Output pointer that will be populated with the number of required
//      instance extensions.
//
// OutExtensionProperties:
//      Output pointer that will be populated with an array of
//      VkExtensionProperties structures required by the given feature.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(const NVSDK_NGX_FeatureDiscoveryInfo *FeatureDiscoveryInfo,
                                                                                                   uint32_t *OutExtensionCount,
                                                                                                   VkExtensionProperties **OutExtensionProperties);

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements
// -----------------------------------------------------------------------------
// Identifies Vulkan device extensions required for NGX feature support.
//
// DESCRIPTION:
//      This utility function identifies the Vulkan device extensions required
//      for a specific NGX feature, specified by its feature ID, and a given
//      VkInstance and VkPhysicalDevice. The function populates
//      OutExtensionCount with the number of extensions required for the NGX
//      feature. It also populates OutExtensionProperties with a pointer to an
//      array of VkExtensionProperties structures, which contains details about
//      each required extension. The size of this array is equal to
//      OutExtensionCount.
//
//      NVSDK_NGX_Init does NOT need to be called before calling this function.
//      This function should be called before the VkDevice for the application
//      is created so that the correct extensions can be enabled.
//
// PARAMETERS:
// Instance:
//      VkInstance in use by the application.
//
// InPhysicalDevice:
//      VkPhysicalDevice in use by the application.
//
// FeatureDiscoveryInfo:
//      Struct containing information about the feature to query and how it is
//      expected to be initialized, including the feature ID and information
//      that would be provided to NVSDK_NGX_Init.
//
// OutExtensionCount:
//      Output pointer that will be populated with the number of required
//      device extensions.
//
// OutExtensionProperties:
//      Output pointer that will be populated with an array of
//      VkExtensionProperties structures required by the given feature.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure.
//
#if defined(NGX_SNIPPET_BUILD)
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(const VkInstance Instance,
                                                                                                 const VkPhysicalDevice PhysicalDevice,
                                                                                                 const NVSDK_NGX_FeatureDiscoveryInfo *FeatureDiscoveryInfo,
                                                                                                 uint32_t *OutExtensionCount,
                                                                                                 VkExtensionProperties **OutExtensionProperties);
#else
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(VkInstance Instance,
                                                                                                 VkPhysicalDevice PhysicalDevice,
                                                                                                 const NVSDK_NGX_FeatureDiscoveryInfo *FeatureDiscoveryInfo,
                                                                                                 uint32_t *OutExtensionCount,
                                                                                                 VkExtensionProperties **OutExtensionProperties);
#endif // defined(NGX_SNIPPET_BUILD)

////////////////////////////////////////////////////////////////////////////////
// NVSDK_NGX_EvaluateFeature
// -----------------------------------------------------------------------------
// Evaluates the specified NGX feature using the provided parameters.
//
// DESCRIPTION:
//      Evaluates the given feature using the provided parameters. The
//      parameters required to evaluate a feature vary. Refer to the feature's
//      documentation and sample code to determine which input parameters are
//      needed.
//
//      To evaluate a feature, an NVSDK_NGX_Handle to an instance of that
//      feature, created with NVSDK_NGX_CreateFeature, must be provided.
//
//      For most features, it is beneficial to pass as many input buffers and
//      parameters as possible (e.g., provide all render targets like color,
//      albedo, normals, depth, etc.). This will ensure maximum future
//      compatibility with new updates.
//
// PARAMETERS:
// InCmdBuffer:
//      Command buffer to use to execute GPU commands. Must be open and
//      recording.
//
// InFeatureHandle:
//      Handle representing the feature to be evaluated.
//
// InParameters:
//      List of parameters required to evaluate the feature.
//
// InCallback:
//      Optional callback for features that might take longer to execute. If
//      specified, the SDK will call it with progress values in the range 0.0f -
//      1.0f. Not all features support progress callbacks. Refer to the
//      documentation for the specific feature to determine whether or not
//      progress callbacks are supported.
//
// RETURNS:
//      NVSDK_NGX_Result indicating success or failure. Common values include:
//        - NVSDK_NGX_Result_Success if the function executed successfully.
//        - NVSDK_NGX_Result_FAIL_InvalidParameter if any of the input
//          parameters are missing or invalid.
//        - NVSDK_NGX_Result_FAIL_FeatureNotFound if a feature with the provided
//          handle could not be found.
//      Check the NGX logs for additional information about any failures.
//
#ifdef __cplusplus
typedef void (NVSDK_CONV *PFN_NVSDK_NGX_ProgressCallback)(float InCurrentProgress, bool &OutShouldCancel);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_EvaluateFeature(VkCommandBuffer InCmdList, const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback = NULL);
#endif
typedef void (NVSDK_CONV *PFN_NVSDK_NGX_ProgressCallback_C)(float InCurrentProgress, bool *OutShouldCancel);
NVSDK_NGX_API NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_VULKAN_EvaluateFeature_C(VkCommandBuffer InCmdList, const NVSDK_NGX_Handle *InFeatureHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback_C InCallback);

#if defined(NGX_SNIPPET_BUILD)
// No NGX Core <---> Snippet interfaces
#else
// NGX return-code conversion-to-string utility only as a helper for debugging/logging - not for official use.
const wchar_t* NVSDK_CONV GetNGXResultAsString(NVSDK_NGX_Result InNGXResult);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // #define NVSDK_NGX_VK_H
