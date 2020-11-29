////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __RENDER_D3D11RENDERCONTEXT_H__
#define __RENDER_D3D11RENDERCONTEXT_H__


#include <NsCore/Noesis.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/Ptr.h>
#include <NsCore/Vector.h>
#include <NsCore/Queue.h>
#include <NsRender/RenderContext.h>

#ifndef NS_PLATFORM_XBOX_ONE
    #include <d3d11.h>
#endif


namespace Noesis { class RenderDevice; }

namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
class D3D11RenderContext final: public RenderContext
{
public:
    D3D11RenderContext();
    ~D3D11RenderContext();

    /// From RenderContext
    //@{
    const char* Description() const override;
    uint32_t Score() const override;
    bool Validate() const override;
    void Init(void* window, uint32_t& samples, bool vsync, bool sRGB) override;
    void Shutdown() override;
    Noesis::RenderDevice* GetDevice() const override;
    void BeginRender() override;
    void EndRender() override;
    void Resize() override;
    float GetGPUTimeMs() const override;
    void SetClearColor(float r, float g, float b, float a) override;
    void SetDefaultRenderTarget(uint32_t width, uint32_t height, bool doClearColor) override;
    Noesis::Ptr<Image> CaptureRenderTarget(Noesis::RenderTarget* surface) const override;
    void Swap() override;
    //@}

private:
    DXGI_SAMPLE_DESC GetSampleDesc(uint32_t samples) const;
    void CreateSwapChain(void* window, uint32_t& samples, bool sRGB);
    void CreateBuffers();
    void CreateQueries();

private:
    Noesis::Ptr<Noesis::RenderDevice> mRenderer;

#ifdef NS_PLATFORM_WINDOWS_DESKTOP
    HMODULE mD3D11;
    PFN_D3D11_CREATE_DEVICE D3D11CreateDevice = nullptr;
#endif

#ifndef NS_PLATFORM_XBOX_ONE
    HWND mHwnd;
    ID3D11Device* mDevice = nullptr;
    ID3D11DeviceContext* mContext = nullptr;
    IDXGISwapChain* mSwapChain = nullptr;
    ID3D11DepthStencilView* mDepthStencil = nullptr;
    ID3D11RenderTargetView* mRenderTarget = nullptr;
#endif

    bool mVSync;
    float mClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    D3D11_VIEWPORT mViewport;

    struct Frame
    {
        ID3D11Query* begin;
        ID3D11Query* end;
        ID3D11Query* disjoint;
    };

    Frame mFrames[6] = {};
    uint32_t mReadFrame = 0;
    uint32_t mWriteFrame = 0;
    float mGPUTime = 0.0f;

    NS_DECLARE_REFLECTION(D3D11RenderContext, RenderContext)
};

}

#endif
