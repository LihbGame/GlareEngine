////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __RENDER_MTLRENDERDEVICE_H__
#define __RENDER_MTLRENDERDEVICE_H__


#include <NsRender/RenderDevice.h>
#include <NsCore/HashMap.h>

#include <Metal/Metal.h>


class MTLTexture;


namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Apple Metal render device
///
/// Metal device requires setting a command buffer and a command encoder per frame. The command
/// buffer must be set before any rendering. Offscreen command encoders will be created there. The
/// command encoder is used for the on-screen rendering phase.
///
/// .. code-block:: c++
///
///    renderer->UpdateRenderTree();
///    device->SetCommandBuffer(commands);
///    renderer->RenderOffscreen();
///
///    commandEncoder = [commands renderCommandEncoderWithDescriptor:passDescriptor];
///    device->SetOnScreenEncoder(commandEncoder, colorFormat, stencilFormat, sampleCount);
///    renderer->Render();
///
////////////////////////////////////////////////////////////////////////////////////////////////////
class MTLRenderDevice final: public Noesis::RenderDevice
{
public:
    MTLRenderDevice(id<MTLDevice> device, bool sRGB);
    ~MTLRenderDevice();

    /// Creates a Noesis texture from a Metal texture
    static Noesis::Ptr<Noesis::Texture> WrapTexture(id<MTLTexture> texture, uint32_t width,
        uint32_t height, uint32_t levels, bool isInverted);

    /// Warms up pipeline cache with the given combination of surface formats
    void PreCreatePipeline(MTLPixelFormat colorFormat, MTLPixelFormat stencilFormat,
        uint32_t sampleCount);

    /// Sets the active command buffer. This method must be invoked per frame and before rendering
    void SetCommandBuffer(id<MTLCommandBuffer> commands);

    /// The passed encoder will be used to generate the commands for the onscreen rendering
    /// This method must be invoked per frame and before the main rendering
    void SetOnScreenEncoder(id<MTLRenderCommandEncoder> encoder, MTLPixelFormat colorFormat,
        MTLPixelFormat stencilFormat, uint32_t sampleCount);

private:
    /// From RenderDevice
    //@{
    const Noesis::DeviceCaps& GetCaps() const override;
    Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget(const char* label, uint32_t width,
        uint32_t height, uint32_t sampleCount) override;
    Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget(const char* label,
        Noesis::RenderTarget* surface) override;
    Noesis::Ptr<Noesis::Texture> CreateTexture(const char* label, uint32_t width, uint32_t height,
        uint32_t numLevels, Noesis::TextureFormat::Enum format) override;
    void UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height, const void* data) override;
    void BeginRender(bool offscreen) override;
    void SetRenderTarget(Noesis::RenderTarget* surface) override;
    void BeginTile(const Noesis::Tile& tile, uint32_t surfaceWidth,
        uint32_t surfaceHeight) override;
    void EndTile() override;
    void ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles,
        uint32_t numTiles) override;
    void EndRender() override;
    void* MapVertices(uint32_t bytes) override;
    void UnmapVertices() override;
    void* MapIndices(uint32_t bytes) override;
    void UnmapIndices() override;
    void DrawBatch(const Noesis::Batch& batch) override;
    //@}

    void InvalidateStateCache();

    void SetRenderPipelineState(id<MTLRenderPipelineState> pipeline);
    void SetDepthStencilState(id<MTLDepthStencilState> state, uint32_t stencilRef);
    void SetFillMode(MTLTriangleFillMode fillMode);
    void SetRenderState(const Noesis::Batch& batch);
    void SetBuffers(const Noesis::Batch& batch);
    void SetFragmentTexture(uint32_t index, id<MTLTexture> texture);
    void SetFragmentSamplerState(uint32_t index, id<MTLSamplerState> sampler);
    void SetTexture(uint32_t index, MTLTexture* texture, Noesis::SamplerState state);
    void SetTextures(const Noesis::Batch& batch);

    id<MTLTexture> CreateAttachment(uint32_t width, uint32_t height, uint32_t sampleCount,
        MTLPixelFormat format, MTLTextureUsage usage) const;
    Noesis::Ptr<Noesis::RenderTarget> CreateRenderTarget(const char* label, uint32_t width,
        uint32_t height, MTLPixelFormat format, id<MTLTexture> colorAA,
        id<MTLTexture> stencil) const;
    void CreateStateObjects();
    void CreateShaders();
    void CreatePipelines(MTLPixelFormat colorFormat, MTLPixelFormat stencilFormat, uint8_t sampleCount);

    struct Page
    {
        id<MTLBuffer> object;
        bool inUse;
        Page* next;
    };

    struct DynamicBuffer
    {
        uint32_t size;
        uint32_t pos;
        uint32_t drawPos;

        Page* currentPage;
        Page* freePages;
        Page* pendingPages;

        uint32_t numPages;
        Page pages[16];
    };

    Page* AllocatePage(DynamicBuffer& buffer);
    void CreateBuffer(DynamicBuffer& buffer, uint32_t size);
    void CreateBuffers();
    void* MapBuffer(DynamicBuffer& buffer, uint32_t size);

    void CheckMultisample();
    void FillCaps(bool sRGB);
    void BindAPI();

private:
    Class MTLTextureDescriptorClass;
    Class MTLRenderPipelineDescriptorClass;
    Class MTLRenderPassDescriptorClass;
    Class MTLDepthStencilDescriptorClass;
    Class MTLVertexDescriptorClass;
    Class MTLSamplerDescriptorClass;

    id<MTLDevice> mDevice;
    id<MTLCommandBuffer> mCommands;
    id<MTLRenderCommandEncoder> mCommandEncoder;

    DynamicBuffer mDynamicVB;
    DynamicBuffer mDynamicIB;

    id<MTLFunction> mVertexClearFunc;
    id<MTLFunction> mFragmentClearFunc;
    id<MTLFunction> mVertexFuncs[7];
    id<MTLFunction> mFragmentFuncs[17];

    typedef NsHashMap<uint64_t, id<MTLRenderPipelineState>> Pipelines;
    Pipelines mPipelines;
    Pipelines mClearPipelines;
    uint64_t mActiveFormat;

    MTLTextureDescriptor* mTextureDesc;

    id<MTLSamplerState> mSamplers[64];
    id<MTLDepthStencilState> mDepthStencilStates[5];

    struct TextureIndex
    {
        enum Enum
        {
            Pattern,
            Ramps,
            Image,
            Glyphs,

            Count
        };
    };

    id<MTLRenderPipelineState> mCachedPipelineState;
    id <MTLSamplerState> mCachedSamplerStates[TextureIndex::Count];
    id <MTLTexture> mCachedTextures[TextureIndex::Count];
    MTLTriangleFillMode mCachedFillMode;
    id <MTLDepthStencilState> mCachedDepthStencilState;
    uint32_t mCachedStencilRef;
    uint32_t mCachedVertexCBHash;
    uint32_t mCachedFragmentCBHash;
    id<MTLBuffer> mCachedVertexBuffer;

    uint8_t mSampleCounts[16];
    Noesis::DeviceCaps mCaps;
};

}

#endif
