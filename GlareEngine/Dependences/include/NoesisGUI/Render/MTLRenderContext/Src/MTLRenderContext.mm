////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "MTLRenderContext.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/Category.h>
#include <NsCore/HighResTimer.h>
#include <NsCore/Log.h>
#include <NsRender/Image.h>
#include <NsRender/MTLFactory.h>
#include <NsRender/Texture.h>
#include <NsRender/RenderTarget.h>

#include <dlfcn.h>

#ifdef NS_PLATFORM_IPHONE
#include <UIKit/UIKit.h>
#endif

#ifdef NS_PLATFORM_OSX
#include <AppKit/AppKit.h>
#endif


using namespace Noesis;
using namespace NoesisApp;


#define MTL_RELEASE(o) \
    NS_MACRO_BEGIN \
        [o release]; \
    NS_MACRO_END

#ifdef NS_PROFILE
    #define MTL_NAME(o, name) \
        NS_MACRO_BEGIN \
            o.label = name; \
        NS_MACRO_END
#else
    #define MTL_NAME(o, name) NS_NOOP
#endif

typedef id<MTLDevice> (*MTLCreateSystemDefaultDeviceFunc)();

////////////////////////////////////////////////////////////////////////////////////////////////////
MTLRenderContext::MTLRenderContext(): mDevice(0), mDrawable(0), mCommandQueue(0), mCommandBuffer(0),
    mCommandEncoder(0), mMetalLayer(0), mPassDescriptor(0), mGPUTime(0.0f)
{
    mMetalBundle = [NSBundle bundleWithPath: @"/System/Library/Frameworks/Metal.framework"];
    if (mMetalBundle != nullptr)
    {
        [mMetalBundle load];
        MTLTextureDescriptorClass = NSClassFromString(@"MTLTextureDescriptor");
        MTLRenderPassDescriptorClass = NSClassFromString(@"MTLRenderPassDescriptor");
        mDevice = ((MTLCreateSystemDefaultDeviceFunc)dlsym(dlopen(0, RTLD_LOCAL | RTLD_LAZY),
            "MTLCreateSystemDefaultDevice"))();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
MTLRenderContext::~MTLRenderContext()
{
    MTL_RELEASE(mDevice);
    [mMetalBundle unload];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* MTLRenderContext::Description() const
{
    return "Metal";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t MTLRenderContext::Score() const
{
    return 200;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MTLRenderContext::Validate() const
{
    return mDevice != nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::Init(void* window, uint32_t& samples, bool vsync, bool sRGB)
{
    NS_LOG_DEBUG("Creating Metal render context");
    NS_LOG_DEBUG(" MTLDevice: %s", [mDevice.name UTF8String]);

    mView = window;
    mSamples = SupportedSampleCount(samples);

    mMetalLayer = [CAMetalLayer layer];
    mMetalLayer.device = mDevice;
    mMetalLayer.pixelFormat = sRGB ? MTLPixelFormatBGRA8Unorm_sRGB : MTLPixelFormatBGRA8Unorm;
    mMetalLayer.framebufferOnly = YES;

#ifdef NS_PLATFORM_IPHONE
    UIView* view = (UIView*)window;
    [view.layer addSublayer: mMetalLayer];
#endif

#ifdef NS_PLATFORM_OSX
    NSView* view = (NSView*)window;
    if (@available(macOS 10.13, *))
    {
        mMetalLayer.displaySyncEnabled = vsync;
    }
    [view setLayer:mMetalLayer];
#endif

    MTL_RELEASE(mMetalLayer);

    mCommandQueue = [mDevice newCommandQueue];
    MTL_NAME(mCommandQueue, @"Noesis::CommandQueue");

    mPassDescriptor = [[MTLRenderPassDescriptorClass renderPassDescriptor] retain];
    mPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

    if (mSamples > 1)
    {
        mPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;
    }
    else
    {
        mPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    }

    mPassDescriptor.stencilAttachment.clearStencil = 0;
    mPassDescriptor.stencilAttachment.loadAction = MTLLoadActionClear;
    mPassDescriptor.stencilAttachment.storeAction = MTLStoreActionDontCare;

    mRenderer = MTLFactory::CreateDevice(mDevice, sRGB);
    MTLFactory::PreCreatePipeline(mRenderer, mMetalLayer.pixelFormat, MTLPixelFormatStencil8, samples);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::Shutdown()
{
    mRenderer.Reset();
    MTL_RELEASE(mPassDescriptor);
    MTL_RELEASE(mCommandQueue);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
RenderDevice* MTLRenderContext::GetDevice() const
{
    return mRenderer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::BeginRender()
{
    mCommandBuffer = [mCommandQueue commandBuffer];
    MTL_NAME(mCommandBuffer, @"Noesis::Commands");

#if defined(NS_PROFILE) && defined(NS_PLATFORM_IPHONE)
    if (@available(iOS 10.3, *))
    {
        [mCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commands)
        {
            mGPUTime = float(commands.GPUEndTime - commands.GPUStartTime);
        }];
    }
#endif

    MTLFactory::SetCommandBuffer(mRenderer, mCommandBuffer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::EndRender()
{
    [mCommandEncoder endEncoding];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::Resize()
{
#ifdef NS_PLATFORM_IPHONE
    UIView* view = (UIView*)mView;
    const CGFloat scale = [UIScreen mainScreen].scale;
    const CGFloat width = view.bounds.size.width * scale;
    const CGFloat height = view.bounds.size.height * scale;
    mMetalLayer.frame = view.layer.frame;
#endif

#ifdef NS_PLATFORM_OSX
    NSView* view = (NSView*)mView;
    const CGFloat width = view.bounds.size.width;
    const CGFloat height = view.bounds.size.height;
#endif

    mMetalLayer.drawableSize = CGSizeMake(width, height);
    MTLTextureDescriptor* desc = [[MTLTextureDescriptorClass alloc] init];

    // Color AA attachment
    if (mSamples > 1)
    {
        desc.textureType = MTLTextureType2DMultisample;
        desc.pixelFormat = mMetalLayer.pixelFormat;
        desc.width = (NSUInteger)width;
        desc.height = (NSUInteger)height;
        desc.sampleCount = mSamples;

        if (@available(macOS 10.11, iOS 9, *))
        {
            desc.resourceOptions = MTLResourceStorageModePrivate;
            desc.storageMode = MTLStorageModePrivate;
            desc.usage = MTLTextureUsageRenderTarget;
        }

#ifdef NS_PLATFORM_IPHONE
        if ([mDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3])
        {
            desc.storageMode = MTLStorageModeMemoryless;
        }
#endif

        id<MTLTexture> colorAA = [mDevice newTextureWithDescriptor:desc];
        MTL_NAME(colorAA, @"Noesis::ColorAA");
        mPassDescriptor.colorAttachments[0].texture = colorAA;
        MTL_RELEASE(colorAA);
    }

    // Stencil attachment
    desc.textureType = mSamples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D;
    desc.pixelFormat = MTLPixelFormatStencil8;
    desc.width = (NSUInteger)width;
    desc.height = (NSUInteger)height;
    desc.sampleCount = mSamples;

    if (@available(macOS 10.11, iOS 9, *))
    {
        desc.resourceOptions = MTLResourceStorageModePrivate;
        desc.storageMode = MTLStorageModePrivate;
        desc.usage = MTLTextureUsageRenderTarget;
    }

#ifdef NS_PLATFORM_IPHONE
    if ([mDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v3])
    {
        desc.storageMode = MTLStorageModeMemoryless;
    }
#endif

    id<MTLTexture> stencil = [mDevice newTextureWithDescriptor:desc];
    MTL_NAME(stencil, @"Noesis::Stencil");
    mPassDescriptor.stencilAttachment.texture = stencil;
    MTL_RELEASE(stencil);

    MTL_RELEASE(desc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float MTLRenderContext::GetGPUTimeMs() const
{
    return mGPUTime * 1000.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::SetClearColor(float r, float g, float b, float a)
{
    mPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(r, g, b, a);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::SetDefaultRenderTarget(uint32_t, uint32_t, bool doClearColor)
{
    mDrawable = mMetalLayer.nextDrawable;
    MTL_NAME(mDrawable.texture, @"Noesis::Color");

    MTLRenderPassColorAttachmentDescriptor* attachment = mPassDescriptor.colorAttachments[0];
    attachment.loadAction = doClearColor ? MTLLoadActionClear : MTLLoadActionDontCare;

    if (attachment.storeAction == MTLStoreActionMultisampleResolve)
    {
        attachment.resolveTexture = mDrawable.texture;
    }
    else
    {
        attachment.texture = mDrawable.texture;
    }

    mCommandEncoder = [mCommandBuffer renderCommandEncoderWithDescriptor:mPassDescriptor];
    MTL_NAME(mCommandEncoder, @"Noesis::RenderCommandEncoder");

    MTLFactory::SetOnScreenEncoder(mRenderer, mCommandEncoder, mMetalLayer.pixelFormat,
        MTLPixelFormatStencil8, (uint8_t)mSamples); // NO MOLA ESTE CAST
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::Image> MTLRenderContext::CaptureRenderTarget(RenderTarget* surface) const
{
    struct MTLTexture: public Texture
    {
        id<MTLTexture> object;
    };

    id<MTLTexture> gpuTexture = ((MTLTexture*)surface->GetTexture())->object;
    uint32_t width = (uint32_t)gpuTexture.width;
    uint32_t height = (uint32_t)gpuTexture.height;

    MTLTextureDescriptor* desc = [MTLTextureDescriptorClass texture2DDescriptorWithPixelFormat:
        gpuTexture.pixelFormat width:width height:height mipmapped:NO];
    id<MTLTexture> cpuTexture = [mDevice newTextureWithDescriptor:desc];

    MTLOrigin origin = {0, 0, 0};
    MTLSize size = {width, height, 1};
    id<MTLBlitCommandEncoder> blitEncoder = [mCommandBuffer blitCommandEncoder];
    [blitEncoder copyFromTexture:gpuTexture sourceSlice:0 sourceLevel:0 sourceOrigin:origin
        sourceSize:size toTexture:cpuTexture destinationSlice:0 destinationLevel:0
        destinationOrigin:origin];
    [blitEncoder endEncoding];

    [mCommandBuffer commit];
    [mCommandBuffer waitUntilCompleted];

    Ptr<Image> image = *new Image(width, height);
    MTLRegion r = {{0, 0}, {width, height, 1}};
    [cpuTexture getBytes:image->Data() bytesPerRow:width*4 fromRegion:(r) mipmapLevel:0];
    MTL_RELEASE(cpuTexture);
    return image;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MTLRenderContext::Swap()
{
    [mCommandBuffer presentDrawable:mDrawable];
    [mCommandBuffer commit];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t MTLRenderContext::SupportedSampleCount(uint32_t samples) const
{
    while (![mDevice supportsTextureSampleCount:samples])
    {
        samples--;
    }

    return samples;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMPLEMENT_REFLECTION(MTLRenderContext)
{
    NsMeta<TypeId>("MTLRenderContext");
    NsMeta<Category>("RenderContext");
}
