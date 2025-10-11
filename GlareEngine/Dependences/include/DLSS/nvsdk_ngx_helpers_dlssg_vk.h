#pragma once
/*
* Copyright (c) 2021-2024 NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software, related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is strictly
* prohibited.
*
* TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
* SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
* LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
* BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
* INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGES.
*/


#ifndef NVSDK_NGX_HELPERS_DLSSG_VK_H
#define NVSDK_NGX_HELPERS_DLSSG_VK_H

#include <vulkan/vulkan.h>
#include "nvsdk_ngx_defs_dlssg.h"
#include "nvsdk_ngx_params_dlssg.h"
#include "nvsdk_ngx_vk.h" // VK here

typedef struct NVSDK_NGX_VK_DLSSG_Eval_Params
{
    NVSDK_NGX_Resource_VK* pBackbuffer;
    NVSDK_NGX_Resource_VK* pDepth;
    NVSDK_NGX_Resource_VK* pMVecs;
    NVSDK_NGX_Resource_VK* pHudless;                        // Optional
    NVSDK_NGX_Resource_VK* pUI;                             // Optional
    NVSDK_NGX_Resource_VK* pNoPostProcessingColor;          // Optional
    NVSDK_NGX_Resource_VK* pBidirectionalDistortionField;   // Optional
    NVSDK_NGX_Resource_VK* pOutputInterpFrame;
    NVSDK_NGX_Resource_VK* pOutputRealFrame;                // Optional. In some cases, the feature may modify this frame (e.g. debugging)
    NVSDK_NGX_Resource_VK* pOutputDisableInterpolation;     // Optional
} NVSDK_NGX_VK_DLSSG_Eval_Params;

static inline NVSDK_NGX_Result NGX_VK_CREATE_DLSSG(
    VkCommandBuffer pInCmdBuf,
    unsigned int InCreationNodeMask,
    unsigned int InVisibilityNodeMask,
    NVSDK_NGX_Handle** ppOutHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_DLSSG_Create_Params* pInDlssgCreateParams)
{
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_CreationNodeMask, InCreationNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_VisibilityNodeMask, InVisibilityNodeMask);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Width, pInDlssgCreateParams->Width);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_Parameter_Height, pInDlssgCreateParams->Height);
    NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BackbufferFormat, pInDlssgCreateParams->NativeBackbufferFormat);

    return NVSDK_NGX_VULKAN_CreateFeature(pInCmdBuf, NVSDK_NGX_Feature_FrameGeneration, pInParams, ppOutHandle);
}

static inline NVSDK_NGX_Result NGX_VK_EVALUATE_DLSSG(
    VkCommandBuffer pInCmdBuf,
    NVSDK_NGX_Handle* pInHandle,
    NVSDK_NGX_Parameter* pInParams,
    NVSDK_NGX_VK_DLSSG_Eval_Params* pInDlssgEvalParams,
    NVSDK_NGX_DLSSG_Opt_Eval_Params* pInDlssgOptEvalParams)
{
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_Backbuffer, pInDlssgEvalParams->pBackbuffer);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_MVecs, pInDlssgEvalParams->pMVecs);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_Depth, pInDlssgEvalParams->pDepth);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_HUDLess, pInDlssgEvalParams->pHudless);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_UI, pInDlssgEvalParams->pUI);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_NoPostProcessingColor, pInDlssgEvalParams->pNoPostProcessingColor);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField, pInDlssgEvalParams->pBidirectionalDistortionField);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_OutputInterpolated, pInDlssgEvalParams->pOutputInterpFrame);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_Parameter_OutputReal, pInDlssgEvalParams->pOutputRealFrame);
    NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_OutputDisableInterpolation, pInDlssgEvalParams->pOutputDisableInterpolation);

    if (pInDlssgOptEvalParams)
    {
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MultiFrameCount, pInDlssgOptEvalParams->multiFrameCount);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MultiFrameIndex, pInDlssgOptEvalParams->multiFrameIndex);

        NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraViewToClip, pInDlssgOptEvalParams->cameraViewToClip);
        NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_ClipToCameraView, pInDlssgOptEvalParams->clipToCameraView);
        NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_ClipToLensClip, pInDlssgOptEvalParams->clipToLensClip);
        NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_ClipToPrevClip, pInDlssgOptEvalParams->clipToPrevClip);
        NVSDK_NGX_Parameter_SetVoidPointer(pInParams, NVSDK_NGX_DLSSG_Parameter_PrevClipToClip, pInDlssgOptEvalParams->prevClipToClip);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_JitterOffsetX, pInDlssgOptEvalParams->jitterOffset[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_JitterOffsetY, pInDlssgOptEvalParams->jitterOffset[1]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_MvecScaleX, pInDlssgOptEvalParams->mvecScale[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_MvecScaleY, pInDlssgOptEvalParams->mvecScale[1]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraPinholeOffsetX, pInDlssgOptEvalParams->cameraPinholeOffset[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraPinholeOffsetY, pInDlssgOptEvalParams->cameraPinholeOffset[1]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraPosX, pInDlssgOptEvalParams->cameraPos[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraPosY, pInDlssgOptEvalParams->cameraPos[1]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraPosZ, pInDlssgOptEvalParams->cameraPos[2]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraUpX, pInDlssgOptEvalParams->cameraUp[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraUpY, pInDlssgOptEvalParams->cameraUp[1]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraUpZ, pInDlssgOptEvalParams->cameraUp[2]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraRightX, pInDlssgOptEvalParams->cameraRight[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraRightY, pInDlssgOptEvalParams->cameraRight[1]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraRightZ, pInDlssgOptEvalParams->cameraRight[2]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraFwdX, pInDlssgOptEvalParams->cameraFwd[0]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraFwdY, pInDlssgOptEvalParams->cameraFwd[1]);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraFwdZ, pInDlssgOptEvalParams->cameraFwd[2]);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraNear, pInDlssgOptEvalParams->cameraNear);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraFar, pInDlssgOptEvalParams->cameraFar);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraFOV, pInDlssgOptEvalParams->cameraFOV);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraAspectRatio, pInDlssgOptEvalParams->cameraAspectRatio);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_ColorBuffersHDR, pInDlssgOptEvalParams->colorBuffersHDR);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_DepthInverted, pInDlssgOptEvalParams->depthInverted);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_CameraMotionIncluded, pInDlssgOptEvalParams->cameraMotionIncluded);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_Reset, pInDlssgOptEvalParams->reset);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_AutomodeOverrideReset, pInDlssgOptEvalParams->automodeOverrideReset);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_NotRenderingGameFrames, pInDlssgOptEvalParams->notRenderingGameFrames);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_OrthoProjection, pInDlssgOptEvalParams->orthoProjection);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_MvecInvalidValue, pInDlssgOptEvalParams->motionVectorsInvalidValue);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MvecDilated, pInDlssgOptEvalParams->motionVectorsDilated);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MenuDetectionEnabled, pInDlssgOptEvalParams->menuDetectionEnabled);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MVecsSubrectBaseX, pInDlssgOptEvalParams->mvecsSubrectBase.X);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MVecsSubrectBaseY, pInDlssgOptEvalParams->mvecsSubrectBase.Y);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MVecsSubrectWidth, pInDlssgOptEvalParams->mvecsSubrectSize.Width);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_MVecsSubrectHeight, pInDlssgOptEvalParams->mvecsSubrectSize.Height);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_DepthSubrectBaseX, pInDlssgOptEvalParams->depthSubrectBase.X);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_DepthSubrectBaseY, pInDlssgOptEvalParams->depthSubrectBase.Y);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_DepthSubrectWidth, pInDlssgOptEvalParams->depthSubrectSize.Width);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_DepthSubrectHeight, pInDlssgOptEvalParams->depthSubrectSize.Height);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectBaseX, pInDlssgOptEvalParams->hudLessSubrectBase.X);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectBaseY, pInDlssgOptEvalParams->hudLessSubrectBase.Y);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectWidth, pInDlssgOptEvalParams->hudLessSubrectSize.Width);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_HUDLessSubrectHeight, pInDlssgOptEvalParams->hudLessSubrectSize.Height);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_UISubrectBaseX, pInDlssgOptEvalParams->uiSubrectBase.X);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_UISubrectBaseY, pInDlssgOptEvalParams->uiSubrectBase.Y);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_UISubrectWidth, pInDlssgOptEvalParams->uiSubrectSize.Width);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_UISubrectHeight, pInDlssgOptEvalParams->uiSubrectSize.Height);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectBaseX, pInDlssgOptEvalParams->bidirectionalDistFieldSubrectBase.X);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectBaseY, pInDlssgOptEvalParams->bidirectionalDistFieldSubrectBase.Y);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectWidth, pInDlssgOptEvalParams->bidirectionalDistFieldSubrectSize.Width);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionFieldSubrectHeight, pInDlssgOptEvalParams->bidirectionalDistFieldSubrectSize.Height);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField_LowPrecision_IsLowPrecision, pInDlssgOptEvalParams->bidirectionalDistFieldPrecisionInfo.IsLowPrecision);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField_LowPrecision_Bias, pInDlssgOptEvalParams->bidirectionalDistFieldPrecisionInfo.Bias);
        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_BidirectionalDistortionField_LowPrecision_Scale, pInDlssgOptEvalParams->bidirectionalDistFieldPrecisionInfo.Scale);

        NVSDK_NGX_Parameter_SetF(pInParams, NVSDK_NGX_DLSSG_Parameter_MinRelativeLinearDepthObjectSeparation, pInDlssgOptEvalParams->minRelativeLinearDepthObjectSeparation);

        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectBaseX,  pInDlssgOptEvalParams->backbufferSubrectBase.X);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectBaseY,  pInDlssgOptEvalParams->backbufferSubrectBase.Y);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectWidth,  pInDlssgOptEvalParams->backbufferSubrectSize.Width);
        NVSDK_NGX_Parameter_SetUI(pInParams, NVSDK_NGX_DLSSG_Parameter_BackbufferSubrectHeight, pInDlssgOptEvalParams->backbufferSubrectSize.Height);
    }

    // C version or cpp?
    return NVSDK_NGX_VULKAN_EvaluateFeature_C(pInCmdBuf, pInHandle, pInParams, NULL);
}

static inline NVSDK_NGX_Result NGX_VK_ESTIMATE_VRAM_DLSSG(
    NVSDK_NGX_Parameter* InParams,
    uint32_t mvecDepthWidth, uint32_t mvecDepthHeight,
    uint32_t colorWidth, uint32_t colorHeight,
    uint32_t colorBufferFormat,
    uint32_t mvecBufferFormat, uint32_t depthBufferFormat,
    uint32_t hudLessBufferFormat, uint32_t uiBufferFormat,
    size_t* estimatedVRAMInBytes
)
{
    void* Callback = NULL;
    NVSDK_NGX_Parameter_GetVoidPointer(InParams, NVSDK_NGX_Parameter_DLSSGEstimateVRAMCallback, &Callback);
    if (!Callback)
    {
        // Possible reasons for this:
        // - Installed feature is out of date and does not support the feature we need
        // - You used NVSDK_NGX_AllocateParameters() for creating InParams. Try using NVSDK_NGX_GetCapabilityParameters() instead
        return NVSDK_NGX_Result_FAIL_OutOfDate;
    }

    NVSDK_NGX_Result Res = NVSDK_NGX_Result_Success;
    PFN_NVSDK_NGX_DLSSG_EstimateVRAMCallback PFNCallback = (PFN_NVSDK_NGX_DLSSG_EstimateVRAMCallback)Callback;
    Res = PFNCallback(mvecDepthWidth, mvecDepthHeight, colorWidth, colorHeight,
        colorBufferFormat, mvecBufferFormat, depthBufferFormat, hudLessBufferFormat, uiBufferFormat, estimatedVRAMInBytes);
    if (NVSDK_NGX_FAILED(Res))
    {
        return Res;
    }

    return NVSDK_NGX_Result_Success;
}

#endif
