////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __RENDER_MTLFACTORY_H__
#define __RENDER_MTLFACTORY_H__


#ifdef __OBJC__


#include <NsCore/Noesis.h>
#include <NsRender/MTLRenderDeviceApi.h>
#include <NsRender/RenderDevice.h>

#include <Metal/Metal.h>


namespace Noesis { template<class T> class Ptr; }

namespace NoesisApp
{

struct NS_RENDER_MTLRENDERDEVICE_API MTLFactory
{
    static Noesis::Ptr<Noesis::RenderDevice> CreateDevice(id<MTLDevice> device, bool sRGB);
    static Noesis::Ptr<Noesis::Texture> WrapTexture(id<MTLTexture> texture, uint32_t width,
        uint32_t height, uint32_t levels, bool isInverted);
    static void PreCreatePipeline(Noesis::RenderDevice* device, MTLPixelFormat colorFormat,
        MTLPixelFormat stencilFormat, uint32_t sampleCount);
    static void SetCommandBuffer(Noesis::RenderDevice* device, id<MTLCommandBuffer> commands);
    static void SetOnScreenEncoder(Noesis::RenderDevice* device, id<MTLRenderCommandEncoder> encoder,
        MTLPixelFormat colorFormat, MTLPixelFormat stencilFormat, uint32_t sampleCount);
};

}

#endif

#endif
