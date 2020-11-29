////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsCore/Package.h>
#include <NsRender/MTLFactory.h>
#include <NsRender/Texture.h>

#include "MTLRenderDevice.h"


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<RenderDevice> MTLFactory::CreateDevice(id<MTLDevice> device, bool sRGB)
{
    return *new MTLRenderDevice(device, sRGB);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Texture> MTLFactory::WrapTexture(id<MTLTexture> texture, uint32_t width, uint32_t height,
    uint32_t levels, bool isInverted)
{
    return MTLRenderDevice::WrapTexture(texture, width, height, levels, isInverted);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLFactory::PreCreatePipeline(RenderDevice* device_, MTLPixelFormat colorFormat,
    MTLPixelFormat stencilFormat, uint32_t sampleCount)
{
    MTLRenderDevice* device = (MTLRenderDevice*)device_;
    device->PreCreatePipeline(colorFormat, stencilFormat, sampleCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLFactory::SetCommandBuffer(RenderDevice* device_, id<MTLCommandBuffer> commands)
{
    MTLRenderDevice* device = (MTLRenderDevice*)device_;
    device->SetCommandBuffer(commands);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLFactory::SetOnScreenEncoder(RenderDevice* device_, id<MTLRenderCommandEncoder> encoder,
    MTLPixelFormat colorFormat, MTLPixelFormat stencilFormat, uint32_t sampleCount)
{
    MTLRenderDevice* device = (MTLRenderDevice*)device_;
    device->SetOnScreenEncoder(encoder, colorFormat, stencilFormat, sampleCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_REGISTER_REFLECTION(Render, MTLRenderDevice)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_INIT_PACKAGE(Render, MTLRenderDevice)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_SHUTDOWN_PACKAGE(Render, MTLRenderDevice)
{
}
