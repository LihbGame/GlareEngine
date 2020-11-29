////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


//#undef NS_MINIMUM_LOG_LEVEL
//#define NS_MINIMUM_LOG_LEVEL 0


#include "GLRenderContext.h"

#include <NsCore/Interface.h>
#include <NsCore/BaseComponent.h>
#include <NsCore/Category.h>
#include <NsCore/Log.h>
#include <NsCore/TypeId.h>
#include <NsCore/Category.h>
#include <NsCore/ReflectionImplement.h>
#include <NsRender/RenderDevice.h>
#include <NsRender/GLFactory.h>
#include <NsRender/Image.h>
#include <NsApp/Display.h>

#if NS_RENDERER_USE_EAGL
#import <OpenGLES/EAGL.h>
#import <QuartzCore/CAEAGLLayer.h>
#import <UIKit/UIKit.h>
#endif

#if NS_RENDERER_USE_NSGL
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSView.h>
#import <QuartzCore/CALayer.h>
#endif


using namespace Noesis;
using namespace NoesisApp;


#if NS_RENDERER_USE_WGL
#define GL_IMPORT(_optional, _proto, _func) \
    { \
        _func = (_proto)wglGetProcAddress(#_func); \
        if (_func == 0) \
        { \
            _func = (_proto)GetProcAddress(mOGL32, #_func); \
        } \
        NS_ASSERT(_func != 0 || _optional); \
    }
#elif NS_RENDERER_USE_EGL
    #define GL_IMPORT(_optional, _proto, _func) \
    { \
        _func = (_proto)eglGetProcAddress(#_func); \
        NS_ASSERT(_func != 0 || _optional); \
    }
#elif NS_RENDERER_USE_GLX
    #define GL_IMPORT(_optional, _proto, _func) \
    { \
        _func = (_proto)glXGetProcAddress((const GLubyte*)#_func); \
        NS_ASSERT(_func != 0 || _optional); \
    }
#endif

#if NS_RENDERER_USE_EGL

#ifdef NS_DEBUG
    #define EGL_CHECK(expr, what) \
        NS_MACRO_BEGIN \
            if (!(expr)) \
            { \
                EGLint err = eglGetError(); \
                switch (err) \
                { \
                    case EGL_SUCCESS: NS_FATAL("%s [EGL_SUCCESS]", what); \
                    case EGL_NOT_INITIALIZED: NS_FATAL("%s [EGL_NOT_INITIALIZED]", what); \
                    case EGL_BAD_ACCESS: NS_FATAL("%s [EGL_BAD_ACCESS]", what); \
                    case EGL_BAD_ALLOC: NS_FATAL("%s [EGL_BAD_ALLOC]", what); \
                    case EGL_BAD_ATTRIBUTE: NS_FATAL("%s [EGL_BAD_ATTRIBUTE]", what); \
                    case EGL_BAD_CONTEXT: NS_FATAL("%s [EGL_BAD_CONTEXT]", what); \
                    case EGL_BAD_CONFIG: NS_FATAL("%s [EGL_BAD_CONFIG]", what); \
                    case EGL_BAD_CURRENT_SURFACE: NS_FATAL("%s [EGL_BAD_CURRENT_SURFACE]", what); \
                    case EGL_BAD_DISPLAY: NS_FATAL("%s [EGL_BAD_DISPLAY]", what); \
                    case EGL_BAD_SURFACE: NS_FATAL("%s [EGL_BAD_SURFACE]", what); \
                    case EGL_BAD_MATCH: NS_FATAL("%s [EGL_BAD_MATCH]", what); \
                    case EGL_BAD_PARAMETER: NS_FATAL("%s [EGL_BAD_PARAMETER]", what); \
                    case EGL_BAD_NATIVE_PIXMAP: NS_FATAL("%s [EGL_BAD_NATIVE_PIXMAP]", what); \
                    case EGL_BAD_NATIVE_WINDOW: NS_FATAL("%s [EGL_BAD_NATIVE_WINDOW]", what); \
                    case EGL_CONTEXT_LOST: NS_FATAL("%s [EGL_CONTEXT_LOST]", what); \
                    default: NS_FATAL("%s [0x%08x]", what, err); \
                } \
            } \
        NS_MACRO_END

    #define EGL_V(exp) \
        NS_MACRO_BEGIN \
            EGLBoolean err = exp; \
            EGL_CHECK(err, #exp); \
        NS_MACRO_END
#else
    #define EGL_CHECK(expr, what)
    #define EGL_V(exp) exp
#endif

#endif

#define NS_GL_VER(major, minor) (major * 10 + minor)

#ifdef NS_DEBUG
    #define V(exp) \
        NS_MACRO_BEGIN \
            while (glGetError() != GL_NO_ERROR); \
            exp; \
            GLenum err = glGetError(); \
            if (err != GL_NO_ERROR) \
            { \
                switch (err) \
                { \
                    case GL_INVALID_ENUM: NS_FATAL("%s [GL_INVALID_ENUM]", #exp); \
                    case GL_INVALID_VALUE: NS_FATAL("%s [GL_INVALID_VALUE]", #exp); \
                    case GL_INVALID_OPERATION: NS_FATAL("%s [GL_INVALID_OPERATION]", #exp); \
                    case GL_INVALID_FRAMEBUFFER_OPERATION: NS_FATAL("%s [GL_INVALID_FRAMEBUFFER_OPERATION]", #exp); \
                    case GL_OUT_OF_MEMORY: NS_FATAL("%s [GL_OUT_OF_MEMORY]", #exp); \
                    default: NS_FATAL("%s [0x%08x]", #exp, err); \
                } \
            } \
        NS_MACRO_END
#else
    #define V(exp) exp
#endif

#ifdef NS_COMPILER_MSVC
    #define sscanf sscanf_s
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
GLRenderContext::GLRenderContext(): GetQueryObjectui64v_(0), mGPUTime(0.0f)
{
#if NS_RENDERER_USE_WGL
    mSurface = 0;
    mContext = 0;
#elif NS_RENDERER_USE_EGL
    mDisplay = 0;
    mSurface = 0;
    mContext = 0;
#elif NS_RENDERER_USE_EMS
    mContext = 0;
#elif NS_RENDERER_USE_EAGL
    mContext = 0;
    mFBO = 0;
    mFBOResolved = 0;
    mColor = 0;
    mColorAA = 0;
    mStencil = 0;
#elif NS_RENDERER_USE_NSGL
    mContext = 0;
#elif NS_RENDERER_USE_GLX
    mContext = 0;
#endif

    mClearColor[0] = 0.0f;
    mClearColor[1] = 0.0f;
    mClearColor[2] = 0.0f;
    mClearColor[3] = 0.0f;

#if NS_RENDERER_USE_WGL
    wglGetProcAddress = 0;
    wglMakeCurrent = 0;
    wglCreateContext = 0;
    wglDeleteContext = 0;

    mOGL32 = LoadLibraryA("opengl32.dll");
    if (mOGL32 != 0)
    {
        wglGetProcAddress = (PFNWGLGETPROCADDRESSPROC)GetProcAddress(mOGL32, "wglGetProcAddress");
        wglMakeCurrent = (PFNWGLMAKECURRENTPROC)GetProcAddress(mOGL32, "wglMakeCurrent");
        wglCreateContext = (PFNWGLCREATECONTEXTPROC)GetProcAddress(mOGL32, "wglCreateContext");
        wglDeleteContext = (PFNWGLDELETECONTEXTPROC)GetProcAddress(mOGL32, "wglDeleteContext");
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GLRenderContext::~GLRenderContext()
{
#if NS_RENDERER_USE_WGL
    if (mOGL32 != 0)
    {
        FreeLibrary(mOGL32);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* GLRenderContext::Description() const
{
#if NS_RENDERER_OPENGL
    return "OpenGL";
#elif NS_RENDERER_USE_EMS
    return "WebGL";
#else
    return "OpenGL ES";
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t GLRenderContext::Score() const
{
    return 100;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GLRenderContext::Validate() const
{
#if NS_RENDERER_USE_WGL
    return wglGetProcAddress != 0 && wglMakeCurrent != 0 && wglCreateContext != 0 &&
        wglDeleteContext != 0;
#else
    return true;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::Init(void* window, uint32_t& sampleCount, bool vsync, bool sRGB)
{
    NS_ASSERT(Validate());
    NS_UNUSED(sRGB);

    NS_LOG_DEBUG("Creating GL render context");

#if NS_RENDERER_USE_EAGL
    // EAGLCreateContext
    mContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (mContext == nil)
    {
        mContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    }

    if (mContext == nullptr)
    {
        NS_FATAL("EAGLCreateContext");
    }

    mRenderingAPI = (uint32_t)[(EAGLContext*)mContext API];
    NS_ASSERT(mRenderingAPI == 2 || mRenderingAPI == 3);

    if (![EAGLContext setCurrentContext:(EAGLContext*)mContext])
    {
        NS_FATAL("EAGLSetCurrentContext");
    }

    V(glGenRenderbuffers(1, &mColor));
    V(glBindRenderbuffer(GL_RENDERBUFFER, mColor));

    // EAGLRenderbufferStorageFromDrawable
    UIView* view = (UIView*)window;
    CAEAGLLayer* layer = [CAEAGLLayer layer];
    layer.frame = view.layer.frame;
    layer.contentsScale = [UIScreen mainScreen].scale;

    [view.layer addSublayer: layer];

    layer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
        kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
        Nil];

    if (![(EAGLContext*)mContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer])
    {
        NS_FATAL("EAGLRenderbufferStorageFromDrawable");
    }

    GLsizei maxSamples;
    V(glGetIntegerv(GL_MAX_SAMPLES, &maxSamples));
    sampleCount = eastl::min_alt((GLsizei)sampleCount, maxSamples);

    GLsizei samples = sampleCount == 1 ? 0 : sampleCount;

    V(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &mWidth));
    V(glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &mHeight));
    GLsizei w = mWidth, h = mHeight;

    V(glGenRenderbuffers(1, &mStencil));
    V(glBindRenderbuffer(GL_RENDERBUFFER, mStencil));
    if (mRenderingAPI == 3)
    {
        V(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_STENCIL_INDEX8, w, h));
    }
    else
    {
        V(glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, samples, GL_STENCIL_INDEX8, w, h));
    }

    const GLenum ColorAttachment = GL_COLOR_ATTACHMENT0;
    const GLenum StencilAttachment = GL_STENCIL_ATTACHMENT;

    if (samples > 1)
    {
        V(glGenRenderbuffers(1, &mColorAA));
        V(glBindRenderbuffer(GL_RENDERBUFFER, mColorAA));
        if (mRenderingAPI == 3)
        {
            V(glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8_OES, w, h));
        }
        else
        {
            V(glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, samples, GL_RGBA8_OES, w, h));
        }

        V(glGenFramebuffers(1, &mFBO));
        V(glBindFramebuffer(GL_FRAMEBUFFER, mFBO));
        V(glFramebufferRenderbuffer(GL_FRAMEBUFFER, ColorAttachment, GL_RENDERBUFFER, mColorAA));
        V(glFramebufferRenderbuffer(GL_FRAMEBUFFER, StencilAttachment, GL_RENDERBUFFER, mStencil));
        NS_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        V(glGenFramebuffers(1, &mFBOResolved));
        V(glBindFramebuffer(GL_FRAMEBUFFER, mFBOResolved));
        V(glFramebufferRenderbuffer(GL_FRAMEBUFFER, ColorAttachment, GL_RENDERBUFFER, mColor));
        NS_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
    else
    {
        V(glGenFramebuffers(1, &mFBO));
        V(glBindFramebuffer(GL_FRAMEBUFFER, mFBO));
        V(glFramebufferRenderbuffer(GL_FRAMEBUFFER, ColorAttachment, GL_RENDERBUFFER, mColor));
        V(glFramebufferRenderbuffer(GL_FRAMEBUFFER, StencilAttachment, GL_RENDERBUFFER, mStencil));
        NS_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }

#elif NS_RENDERER_USE_NSGL
    @autoreleasepool
    {
        NSOpenGLPixelFormatAttribute attrs[] =
        {
            NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
            NSOpenGLPFADoubleBuffer,
            NSOpenGLPFAAccelerated,
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFAMultisample,
            NSOpenGLPFASampleBuffers, sampleCount > 1,
            NSOpenGLPFASamples, sampleCount,
            NSOpenGLPFAAlphaSize, 0,
            NSOpenGLPFADepthSize, 0,
            NSOpenGLPFAStencilSize, 8,
            0
        };

        NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        if (format == nullptr)
        {
            attrs[1] = NSOpenGLProfileVersionLegacy;
            format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        }

        NS_ASSERT(format != nullptr);

        GLint cSize, aSize, zSize, sSize, msaa;
        [format getValues:&cSize forAttribute:NSOpenGLPFAColorSize forVirtualScreen:0];
        [format getValues:&aSize forAttribute:NSOpenGLPFAAlphaSize forVirtualScreen:0];
        [format getValues:&zSize forAttribute:NSOpenGLPFADepthSize forVirtualScreen:0];
        [format getValues:&sSize forAttribute:NSOpenGLPFAStencilSize forVirtualScreen:0];
        [format getValues:&msaa forAttribute:NSOpenGLPFASamples forVirtualScreen:0];
        sampleCount = eastl::max_alt(1, msaa);

        NS_LOG_DEBUG(" PixelFormat: C%dA%d D%dS%d %dx", cSize, aSize, zSize, sSize, sampleCount);
        mContext = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
        [format release];

        if (mContext == nullptr)
        {
            NS_FATAL("NSGLCreateContext");
        }

        GLint swapInterval = vsync ? 1 : 0;
        [(NSOpenGLContext*)mContext setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];

        // This fixes a buf in macOS 10.14 and Xcode 10 (seems to be fixed in macOS 10.14.2)
        // https://github.com/glfw/glfw/issues/1334
        NSView* view = (NSView*)window;
        CALayer* layer = [CALayer layer];
        view.layer = layer;
        [layer release];

        [(NSOpenGLContext*)mContext setView:(NSView*)window];
        [(NSOpenGLContext*)mContext makeCurrentContext];
    }

#elif NS_RENDERER_USE_WGL
    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = 0;
    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = 0;
    PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = 0;
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 0;
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = 0;

    // Dummy window and context to get ARB wgl func pointers
    {
        HWND hWnd = CreateWindowA("STATIC", "", WS_POPUP | WS_DISABLED, -32000, -32000, 0, 0,
            NULL, NULL, GetModuleHandle(NULL), 0);
        NS_ASSERT(hWnd != 0);

        HDC hdc = GetDC(hWnd);
        NS_ASSERT(hdc != 0);

        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;

        int pixelFormat = ChoosePixelFormat(hdc, &pfd);
        NS_ASSERT(pixelFormat != 0);

        BOOL r = SetPixelFormat(hdc, pixelFormat, &pfd);
        NS_ASSERT(r);

        HGLRC context = wglCreateContext(hdc);
        NS_ASSERT(context != 0);

        r = wglMakeCurrent(hdc, context);
        NS_ASSERT(r);

        GL_IMPORT(true, PFNWGLGETEXTENSIONSSTRINGARBPROC, wglGetExtensionsStringARB);
        GL_IMPORT(true, PFNWGLCHOOSEPIXELFORMATARBPROC, wglChoosePixelFormatARB);
        GL_IMPORT(true, PFNWGLGETPIXELFORMATATTRIBIVARBPROC, wglGetPixelFormatAttribivARB);
        GL_IMPORT(true, PFNWGLCREATECONTEXTATTRIBSARBPROC, wglCreateContextAttribsARB);
        GL_IMPORT(true, PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT);

        wglMakeCurrent(0, 0);
        wglDeleteContext(context);
        ReleaseDC(hWnd, hdc);
        DestroyWindow(hWnd);
    }

    mSurface = GetDC((HWND)window);
    NS_ASSERT(mSurface != 0);

    const char* extensions = wglGetExtensionsStringARB ? wglGetExtensionsStringARB(mSurface) : "";
    int pixelFormat = 0;

    if (strstr(extensions, "WGL_ARB_pixel_format") && strstr(extensions, "WGL_ARB_multisample"))
    {
        int samples = sampleCount;

        do
        {
            int attribs[] =
            {
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB, 24,
                WGL_STENCIL_BITS_ARB, 8,
                WGL_SAMPLE_BUFFERS_ARB, samples > 1 ? 1 : 0,
                WGL_SAMPLES_ARB, samples > 1 ? samples : 0,
                0
            };

            UINT n;
            if (!wglChoosePixelFormatARB(mSurface, attribs, NULL, 1, &pixelFormat, &n) || n == 0)
            {
                pixelFormat = 0;
                samples >>= 1;
            }
        }
        while (pixelFormat == 0 && samples != 0);

        if (pixelFormat != 0)
        {
            int attrib = WGL_SAMPLES_ARB;
            int value;
            BOOL r = wglGetPixelFormatAttribivARB(mSurface, pixelFormat, 0, 1, &attrib, &value);
            NS_ASSERT(r);

            sampleCount = value > 1 ? value : 1;
        }
    }

    if (pixelFormat == 0)
    {
        PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1};
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cStencilBits = 8;
        pfd.cColorBits = 24;
        pixelFormat = ChoosePixelFormat(mSurface, &pfd);

        sampleCount = 1;
    }

    NS_ASSERT(pixelFormat != 0);

    PIXELFORMATDESCRIPTOR pfd;
    BOOL r = DescribePixelFormat(mSurface, pixelFormat, sizeof(pfd), &pfd);
    NS_ASSERT(r);

    NS_LOG_DEBUG(" PixelFormat: R%dG%dB%dA%d D%dS%d %dx", pfd.cRedBits, pfd.cGreenBits,
        pfd.cBlueBits, pfd.cAlphaBits, pfd.cDepthBits, pfd.cStencilBits, sampleCount);

    r = SetPixelFormat(mSurface, pixelFormat, &pfd);
    NS_ASSERT(r);

    if (strstr(extensions, "WGL_ARB_create_context"))
    {
        int versions[] = { 46, 45, 44, 43, 42, 41, 40, 33};
        int flags = 0;

        #ifdef NS_DEBUG
            NS_LOG_WARNING(" Creating a Debug context");
            flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
        #endif

        bool createContextNoError = strstr(extensions, "WGL_ARB_create_context_no_error") != 0;

        for (int i = 0; i < NS_COUNTOF(versions) && mContext == 0; i++)
        {
            int attribs[] =
            {
                WGL_CONTEXT_FLAGS_ARB, flags,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                WGL_CONTEXT_MAJOR_VERSION_ARB, versions[i] / 10,
                WGL_CONTEXT_MINOR_VERSION_ARB, versions[i] % 10,
                0, 0,
                0
            };

            #ifndef NS_DEBUG
                if (createContextNoError)
                {
                    attribs[8] = WGL_CONTEXT_OPENGL_NO_ERROR_ARB;
                    attribs[9] = GL_TRUE;
                }
            #endif

            mContext = wglCreateContextAttribsARB(mSurface, 0, attribs);
        }
    }

    if (mContext == 0)
    {
        mContext = wglCreateContext(mSurface);
    }

    NS_ASSERT(mContext != 0);

    r = wglMakeCurrent(mSurface, mContext);
    NS_ASSERT(r);

    if (strstr(extensions, "WGL_EXT_swap_control"))
    {
        wglSwapIntervalEXT(vsync ? 1 : 0);
    }

    GL_IMPORT(false, PFNGLGETSTRINGPROC, glGetString);
    GL_IMPORT(false, PFNGLGETERRORPROC, glGetError);
    GL_IMPORT(false, PFNGLVIEWPORTPROC, glViewport);
    GL_IMPORT(false, PFNGLPIXELSTOREI, glPixelStorei);
    GL_IMPORT(false, PFNGLREADPIXELSPROC, glReadPixels);
    GL_IMPORT(false, PFNGLGETINTEGERVPROC, glGetIntegerv);
    GL_IMPORT(false, PFNGLENABLEPROC, glEnable);
    GL_IMPORT(false, PFNGLDISABLEPROC, glDisable);
    GL_IMPORT(false, PFNGLCLEARCOLORPROC, glClearColor);
    GL_IMPORT(false, PFNGLCLEARSTENCILPROC, glClearStencil);
    GL_IMPORT(false, PFNGLCLEARPROC, glClear);
    GL_IMPORT(false, PFNGLCOLORMASKPROC, glColorMask);

    GL_IMPORT(false, PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
    GL_IMPORT(true, PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v);
    GL_IMPORT(true, PFNGLGETQUERYOBJECTUI64VEXTPROC, glGetQueryObjectui64vEXT);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECALLBACKARBPROC, glDebugMessageCallbackARB);

    GL_IMPORT(true, PFNGLGETSTRINGIPROC, glGetStringi);
    GL_IMPORT(true, PFNGLGENQUERIESPROC, glGenQueries);
    GL_IMPORT(true, PFNGLDELETEQUERIESPROC, glDeleteQueries);
    GL_IMPORT(true, PFNGLBEGINQUERYPROC, glBeginQuery);
    GL_IMPORT(true, PFNGLENDQUERYPROC, glEndQuery);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);

#elif NS_RENDERER_USE_EGL
    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    NS_ASSERT(mDisplay != EGL_NO_DISPLAY);

    EGL_V(eglInitialize(mDisplay, 0, 0));

    NS_LOG_DEBUG(" EGL Version: %s", eglQueryString(mDisplay, EGL_VERSION));

    bool found = false;
    while (sampleCount != 0)
    {
        // Android emulator is very picky with these values
        const EGLint attrs[] =
        {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BUFFER_SIZE, 32,
            EGL_STENCIL_SIZE, 8,
            EGL_SAMPLE_BUFFERS, sampleCount > 1 ? 1 : 0,
            EGL_SAMPLES, sampleCount > 1 ? (EGLint)sampleCount : 0,
            EGL_NONE
        };

        EGLint numConfigs;
        EGL_V(eglChooseConfig(mDisplay, attrs, &mConfig, 1, &numConfigs));

        if (numConfigs == 1)
        {
            found = true;
            break;
        }

        sampleCount -= 1;
    }

    NS_ASSERT(found);

    EGLint rSize, gSize, bSize, aSize, zSize, sSize, samples;
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_RED_SIZE, &rSize));
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_GREEN_SIZE, &gSize));
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_BLUE_SIZE, &bSize));
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_ALPHA_SIZE, &aSize));
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_DEPTH_SIZE, &zSize));
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_STENCIL_SIZE, &sSize));
    EGL_V(eglGetConfigAttrib(mDisplay, mConfig, EGL_SAMPLES, &samples));
    EGLint cSize = rSize + gSize + bSize; 
    NS_LOG_DEBUG(" PixelFormat: C%dA%d D%dS%d %dx", cSize, aSize, zSize, sSize, sampleCount);

    EGLint attribs[] =
    {
        // This also includes OpenGL ES 3.0 because it is backwards compatible with OpenGL ES 2.0
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE, EGL_NONE,
        EGL_NONE
    };

    const char* extensions = eglQueryString(mDisplay, EGL_EXTENSIONS);

#ifdef NS_DEBUG
    if (String::FindFirst(extensions, "EGL_KHR_config_attribs") != -1)
    {
        attribs[2] = EGL_CONTEXT_FLAGS_KHR;
        attribs[3] = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
    }
#else
    if (String::FindFirst(extensions, "EGL_KHR_create_context_no_error") != -1)
    {
        attribs[2] = EGL_CONTEXT_OPENGL_NO_ERROR_KHR;
        attribs[3] = EGL_TRUE;
    }
#endif

    mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, attribs);
    EGL_CHECK(mContext != EGL_NO_CONTEXT, "eglCreateContext");

#ifdef NS_PLATFORM_LINUX
    SetWindow((*(Window**)window));
#else
    SetWindow(window);
#endif

    EGL_V(eglSwapInterval(mDisplay, vsync ? 1 : 0));

    GL_IMPORT(true, PFNGLGETSTRINGIPROC, glGetStringi);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);
    GL_IMPORT(true, PFNGLGENQUERIESEXTPROC, glGenQueriesEXT);
    GL_IMPORT(true, PFNGLDELETEQUERIESEXTPROC, glDeleteQueriesEXT);
    GL_IMPORT(true, PFNGLBEGINQUERYEXTPROC, glBeginQueryEXT);
    GL_IMPORT(true, PFNGLENDQUERYEXTPROC, glEndQueryEXT);
    GL_IMPORT(true, PFNGLGETQUERYOBJECTUI64VEXTPROC, glGetQueryObjectui64vEXT);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECONTROLKHRPROC, glDebugMessageControlKHR);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECALLBACKKHRPROC, glDebugMessageCallbackKHR);

#elif NS_RENDERER_USE_EMS
    sampleCount = 1;

    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);

    attrs.alpha = false;
    attrs.depth = false;
    attrs.stencil = true;
    attrs.antialias = false;
    attrs.preserveDrawingBuffer = false;
    attrs.majorVersion = 2;

    mContext = emscripten_webgl_create_context(0, &attrs);
    if (mContext <= 0)
    {
        attrs.majorVersion = 1;
        mContext = emscripten_webgl_create_context(0, &attrs);
        if (mContext <= 0)
        {
            NS_FATAL("emscripten_webgl_create_context");
        }
    }

    EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(mContext);
    if (r != EMSCRIPTEN_RESULT_SUCCESS)
    {
        NS_FATAL("emscripten_webgl_make_context_current");
    }

#elif NS_RENDERER_USE_GLX
    struct XWindow
    {
        Window window;
        ::Display* display;
    };

    mSurface = ((XWindow*)window)->window;
    mDisplay = ((XWindow*)window)->display;
    int screen = DefaultScreen(mDisplay);
    
    int errorBase, eventBase;
    if (!glXQueryExtension(mDisplay, &errorBase, &eventBase))
    {
        NS_FATAL("GLX extension not supported");
    }

    int glxMajor, glxMinor;
    glXQueryVersion(mDisplay, &glxMajor, &glxMinor);
    if (glxMajor * 10 + glxMinor < 13)
    {
        NS_FATAL("GLX version must be >=1.3");
    }

    NS_LOG_DEBUG(" Connected to X server '%s' %d.%d", XServerVendor(mDisplay), glxMajor, glxMinor);

    const char* extensions = glXQueryExtensionsString(mDisplay, 0);

    GLXFBConfig config;

    if (sampleCount > 1 && strstr(extensions, "GLX_ARB_multisample"))
    {
        int samples = sampleCount;
        int numConfigs = 0;

        do
        {
            NS_ASSERT(samples != 0);

            const int attrs[] =
            {
                GLX_X_RENDERABLE, GL_TRUE,
                GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
                GLX_RENDER_TYPE, GLX_RGBA_BIT,
                GLX_DOUBLEBUFFER, GL_TRUE,
                GLX_RED_SIZE, 8,
                GLX_BLUE_SIZE, 8,
                GLX_GREEN_SIZE, 8,
                GLX_STENCIL_SIZE, 8,
                GLX_SAMPLE_BUFFERS_ARB, samples > 1 ? 1 : 0,
                GLX_SAMPLES_ARB, samples > 1 ? samples : 0,
                0,
            };

            GLXFBConfig* configs = glXChooseFBConfig(mDisplay, screen, attrs, &numConfigs);

            if (numConfigs == 0)
            {
                samples >>= 1;
            }
            else
            {
                config = configs[0];
                XFree(configs);
            }
        }
        while (numConfigs == 0);

        int value;
        glXGetFBConfigAttrib(mDisplay, config, GLX_SAMPLES_ARB, &value);
        sampleCount = value > 1 ? value : 1;
    }
    else
    {
        sampleCount = 1;

        const int attrs[] =
        {
            GLX_X_RENDERABLE, GL_TRUE,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_DOUBLEBUFFER, GL_TRUE,
            GLX_RED_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_STENCIL_SIZE, 8,
            0,
        };

        int numConfigs;
        GLXFBConfig* configs = glXChooseFBConfig(mDisplay, screen, attrs, &numConfigs);
        NS_ASSERT(numConfigs > 0);

        config = configs[0];
        XFree(configs);
    }

    int r, g, b, a, depth, stencil;
    glXGetFBConfigAttrib(mDisplay, config, GLX_RED_SIZE, &r);
    glXGetFBConfigAttrib(mDisplay, config, GLX_GREEN_SIZE, &g);
    glXGetFBConfigAttrib(mDisplay, config, GLX_BLUE_SIZE, &b);
    glXGetFBConfigAttrib(mDisplay, config, GLX_ALPHA_SIZE, &a);
    glXGetFBConfigAttrib(mDisplay, config, GLX_DEPTH_SIZE, &depth);
    glXGetFBConfigAttrib(mDisplay, config, GLX_STENCIL_SIZE, &stencil);
    NS_LOG_DEBUG(" PixelFormat: C%dA%d D%dS%d %dx", r + g + b, a, depth, stencil, sampleCount);

    XVisualInfo* visualInfo = glXGetVisualFromFBConfig(mDisplay, config);
    mContext = glXCreateContext(mDisplay, visualInfo, None, true);
    NS_ASSERT(mContext != 0);
    XFree(visualInfo);

    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = 0;
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = 0;
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = 0;
    PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = 0;

    GL_IMPORT(true, PFNGLXCREATECONTEXTATTRIBSARBPROC, glXCreateContextAttribsARB);
    GL_IMPORT(true, PFNGLXSWAPINTERVALEXTPROC, glXSwapIntervalEXT);
    GL_IMPORT(true, PFNGLXSWAPINTERVALMESAPROC, glXSwapIntervalMESA);
    GL_IMPORT(true, PFNGLXSWAPINTERVALSGIPROC, glXSwapIntervalSGI);

    if (strstr(extensions, "GLX_ARB_create_context"))
    {
        GLXContext context = nullptr;
        int versions[] = { 46, 45, 44, 43, 42, 41, 40, 33 };
        int flags = 0;

        #ifdef NS_DEBUG
            NS_LOG_WARNING(" Creating a Debug context");
            flags |= GLX_CONTEXT_DEBUG_BIT_ARB;
        #endif

        bool createContextNoError = strstr(extensions, "GLX_ARB_create_context_no_error") != 0;

        for (uint32_t i = 0; i < NS_COUNTOF(versions) && context == 0; i++)
        {
            int attribs[] =
            {
                GLX_CONTEXT_FLAGS_ARB, flags,
                GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                GLX_CONTEXT_MAJOR_VERSION_ARB, versions[i] / 10,
                GLX_CONTEXT_MINOR_VERSION_ARB, versions[i] % 10,
                0, 0,
                0
            };

            #ifndef NS_DEBUG
                if (createContextNoError)
                {
                    attribs[8] = GLX_CONTEXT_OPENGL_NO_ERROR_ARB;
                    attribs[9] = GL_TRUE;
                }
            #endif

            XSetErrorHandler([](::Display*, XErrorEvent*) -> int { return 0; });
            context = glXCreateContextAttribsARB(mDisplay, config, nullptr, true, attribs);
            XSetErrorHandler(nullptr);
        }

        if (context != nullptr)
        {
            glXDestroyContext(mDisplay, mContext);
            mContext = context;
        }
    }

    if (!glXIsDirect(mDisplay, mContext))
    {
        NS_LOG_WARNING(" Indirect GLX rendering context");
    }
    
    if (!glXMakeCurrent(mDisplay, mSurface, mContext))
    {
        NS_FATAL("glXMakeCurrent");
    }

    if (strstr(extensions, "GLX_EXT_swap_control"))
    {
        glXSwapIntervalEXT(mDisplay, mSurface, vsync ? 1 : 0);
    }
    else if (strstr(extensions, "GLX_MESA_swap_control"))
    {
        glXSwapIntervalMESA(vsync ? 1 : 0);
    }
    else if (strstr(extensions, "GLX_SGI_swap_control"))
    {
        glXSwapIntervalSGI(vsync ? 1 : 0);
    }

    GL_IMPORT(false, PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
    GL_IMPORT(true, PFNGLGENQUERIESPROC, glGenQueries);
    GL_IMPORT(true, PFNGLDELETEQUERIESPROC, glDeleteQueries);
    GL_IMPORT(true, PFNGLBEGINQUERYPROC, glBeginQuery);
    GL_IMPORT(true, PFNGLENDQUERYPROC, glEndQuery);
    GL_IMPORT(true, PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v);
    GL_IMPORT(true, PFNGLGETQUERYOBJECTUI64VEXTPROC, glGetQueryObjectui64vEXT);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECALLBACKARBPROC, glDebugMessageCallbackARB);
    GL_IMPORT(true, PFNGLGETSTRINGIPROC, glGetStringi);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl);
    GL_IMPORT(true, PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback);

#else
    #error Unknown Windowing System Interface
#endif

    FindExtensions();

#ifdef NS_PROFILE
    if (HaveQueries())
    {
        V(GenQueries_(NS_COUNTOF(mQueries), mQueries));
        mReadQuery = 0;
        mWriteQuery = 0;
    }
#else
    NS_UNUSED(GenQueries_);
    NS_UNUSED(DeleteQueries_);
    NS_UNUSED(BeginQuery_);
    NS_UNUSED(EndQuery_);
    NS_UNUSED(mQueries);
    NS_UNUSED(mReadQuery);
    NS_UNUSED(mWriteQuery);
#endif

    mDevice = GLFactory::CreateDevice();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::Shutdown()
{
    mDevice.Reset();

#ifdef NS_PROFILE
    if (HaveQueries())
    {
        V(DeleteQueries_(NS_COUNTOF(mQueries), mQueries));
    }
#endif

#if NS_RENDERER_USE_WGL
    if (mContext != 0)
    {
        wglDeleteContext(mContext);
        wglMakeCurrent(0, 0);
    }

    if (mSurface != 0)
    {
        ReleaseDC(WindowFromDC(mSurface), mSurface);
    }

#elif NS_RENDERER_USE_EGL
    if (mDisplay != 0)
    {
        if (mSurface != 0)
        {
            EGL_V(eglDestroySurface(mDisplay, mSurface));
        }
        if (mContext != 0)
        {
            EGL_V(eglDestroyContext(mDisplay, mContext));
            EGL_V(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        }

        EGL_V(eglTerminate(mDisplay));
    }

#elif NS_RENDERER_USE_EMS
    if (mContext != 0)
    {
        emscripten_webgl_destroy_context(mContext);
        emscripten_webgl_make_context_current(0);
    }

#elif NS_RENDERER_USE_EAGL
    V(glDeleteRenderbuffers(1, &mStencil));
    V(glDeleteRenderbuffers(1, &mColor));
    V(glDeleteRenderbuffers(1, &mColorAA));
    V(glDeleteFramebuffers(1, &mFBO));
    V(glDeleteFramebuffers(1, &mFBOResolved));

    if (mContext != 0)
    {
        [(EAGLContext*)mContext release];
        [EAGLContext setCurrentContext:nullptr];
    }

#elif NS_RENDERER_USE_NSGL
    if (mContext != 0)
    {
        [(NSOpenGLContext*)mContext release];
        [NSOpenGLContext clearCurrentContext];
    }

#elif NS_RENDERER_USE_GLX
   if (mContext != 0)
    {
        glXDestroyContext(mDisplay, mContext);
        glXMakeCurrent(mDisplay, 0, 0);
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::SetWindow(void* window)
{
#if NS_RENDERER_USE_EGL
    if (mSurface != 0)
    {
        EGL_V(eglDestroySurface(mDisplay, mSurface));
    }

    mSurface = eglCreateWindowSurface(mDisplay, mConfig, (EGLNativeWindowType)window, 0);
    EGL_CHECK(mSurface != EGL_NO_SURFACE, "eglCreateWindowSurface");

    EGL_V(eglSurfaceAttrib(mDisplay, mSurface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED));
    EGL_V(eglMakeCurrent(mDisplay, mSurface, mSurface, mContext));
#else
    NS_UNUSED(window);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
RenderDevice* GLRenderContext::GetDevice() const
{
    return mDevice;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::BeginRender()
{
#ifdef NS_PROFILE
    if (HaveQueries())
    {
        V(BeginQuery_(GL_TIME_ELAPSED, mQueries[mWriteQuery]));
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::EndRender()
{
#ifdef NS_PROFILE
    if (HaveQueries())
    {
        V(EndQuery_(GL_TIME_ELAPSED));

        mWriteQuery = (mWriteQuery + 1) % NS_COUNTOF(mQueries);
        if (mWriteQuery == mReadQuery)
        {
            GLuint64 time = 0;
            V(GetQueryObjectui64v_(mQueries[mReadQuery], GL_QUERY_RESULT, &time));
            mReadQuery = (mReadQuery + 1) % NS_COUNTOF(mQueries);

            mGPUTime = (float)(double(time) / 1e06);
        }
    }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::Resize()
{
#if NS_RENDERER_USE_NSGL
    [(NSOpenGLContext*)mContext update];
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float GLRenderContext::GetGPUTimeMs() const
{
    return mGPUTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::SetClearColor(float r, float g, float b, float a)
{
    mClearColor[0] = r;
    mClearColor[1] = g;
    mClearColor[2] = b;
    mClearColor[3] = a;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::SetDefaultRenderTarget(uint32_t width, uint32_t height, bool doClearColor)
{
#if NS_RENDERER_USE_EAGL
    V(glBindFramebuffer(GL_FRAMEBUFFER, mFBO));
#else
    V(glBindFramebuffer(GL_FRAMEBUFFER, 0));
#endif

    V(glViewport(0, 0, width, height));

    V(glDisable(GL_SCISSOR_TEST));
    V(glClearStencil(0));
    GLbitfield clearMask = GL_STENCIL_BUFFER_BIT;

    if (doClearColor)
    {
        clearMask |= GL_COLOR_BUFFER_BIT;
        V(glColorMask(true, true, true, true));
        V(glClearColor(mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]));
    }

    V(glClear(clearMask));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::Image> GLRenderContext::CaptureRenderTarget(RenderTarget* surface) const
{
    NS_UNUSED(surface);

    GLint viewport[4];
    V(glGetIntegerv(GL_VIEWPORT, viewport));

    uint32_t x = viewport[0];
    uint32_t y = viewport[1];
    uint32_t width = viewport[2];
    uint32_t height = viewport[3];

    Ptr<Image> image = *new Image(width, height);

    V(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    V(glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image->Data()));

    Ptr<Image> dest = *new Image(width, height);

    const uint8_t* src = static_cast<const uint8_t*>(image->Data());
    src += (height - 1) * image->Stride();
    uint8_t* dst = static_cast<uint8_t*>(dest->Data());

    for (uint32_t i = 0; i < height; i++)
    {
        for (uint32_t j = 0; j < width; j++)
        {
            dst[4 * j + 0] = src[4 * j + 2];
            dst[4 * j + 1] = src[4 * j + 1];
            dst[4 * j + 2] = src[4 * j + 0];
            dst[4 * j + 3] = src[4 * j + 3];
        }

        src -= image->Stride();
        dst += dest->Stride();
    }

    return dest;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::Swap()
{
#if NS_RENDERER_USE_WGL
    SwapBuffers(mSurface);

#elif NS_RENDERER_USE_EGL
    EGL_V(eglSwapBuffers(mDisplay, mSurface));

#elif NS_RENDERER_USE_EMS

#elif NS_RENDERER_USE_EAGL
    if (mFBOResolved != 0)
    {
        V(glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, mFBOResolved));
        V(glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE, mFBO));

        if (mRenderingAPI == 3)
        {
            const GLint w = mWidth, h = mHeight;
            V(glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST));

            GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_STENCIL_ATTACHMENT };
            V(glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, 2, attachments));
        }
        else
        {
            V(glResolveMultisampleFramebufferAPPLE());

            GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_STENCIL_ATTACHMENT };
            V(glDiscardFramebufferEXT(GL_READ_FRAMEBUFFER, 2, attachments));
        }
    }

    V(glBindRenderbuffer(GL_RENDERBUFFER, mColor));
    V([(EAGLContext*)mContext presentRenderbuffer:GL_RENDERBUFFER]);

#elif NS_RENDERER_USE_NSGL
    [(NSOpenGLContext*)mContext flushBuffer];

#elif NS_RENDERER_USE_GLX
    glXSwapBuffers(mDisplay, mSurface);

#else
    #error Unknown Windowing System Interface
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t GLRenderContext::GLVersion() const
{
    uint32_t major;
    uint32_t minor;
    const char* version = (const char*)glGetString(GL_VERSION);
    NS_ASSERT(version != 0);

#if NS_RENDERER_OPENGL
    if (sscanf(version, "%d.%d", &major, &minor) != 2)
    {
        NS_DEBUG_BREAK;
        NS_LOG_WARNING("Unable to parse GL_VERSION '%s'", version);
    }
#else
    if (sscanf(version, "OpenGL ES %d.%d", &major, &minor) != 2)
    {
        char profile[2];
        if (sscanf(version, "OpenGL ES-%c%c %d.%d", profile, profile + 1, &major, &minor) != 4)
        {
            NS_DEBUG_BREAK;
            NS_LOG_WARNING("Unable to parse GL_VERSION '%s'", version);
        }
    }
#endif

    NS_LOG_DEBUG(" Version:  %d.%d (%s)", major, minor, version);
    NS_LOG_DEBUG(" Vendor:   %s", glGetString(GL_VENDOR));
    NS_LOG_DEBUG(" Renderer: %s", glGetString(GL_RENDERER));
    return 10 * major + minor;
}

#if !NS_RENDERER_USE_EMS && defined(NS_DEBUG)
#if (GL_VERSION_4_3 || GL_ES_VERSION_3_2 || GL_KHR_debug || GL_ARB_debug_output)

////////////////////////////////////////////////////////////////////////////////////////////////////
static void GL_APIENTRY DebugMessageCallback(GLenum source, GLenum type, GLuint, GLenum severity,
    GLsizei length, const GLchar *message, const GLvoid *)
{
    const char* szSource;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:
        {
            szSource = "API";
            break;
        }
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        {
            szSource = "WINDOW_SYSTEM";
            break;
        }
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
        {
            szSource = "SHADER_COMPILER";
            break;
        }
        case GL_DEBUG_SOURCE_THIRD_PARTY:
        {
            szSource = "THIRD_PARTY";
            break;
        }
        case GL_DEBUG_SOURCE_APPLICATION:
        {
            szSource = "APPLICATION";
            break;
        }
        case GL_DEBUG_SOURCE_OTHER:
        {
            szSource = "OTHER";
            break;
        }
        default:
        {
            szSource = "";
            break;
        }
    }

    const char* szType;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:
        {
            szType = "ERROR";
            break;
        }
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        {
            szType = "DEPRECATED_BEHAVIOR";
            break;
        }
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        {
            szType = "UNDEFINED_BEHAVIOR";
            break;
        }
        case GL_DEBUG_TYPE_PORTABILITY:
        {
            szType = "PORTABILITY";
            break;
        }
        case GL_DEBUG_TYPE_PERFORMANCE:
        {
            szType = "PERFORMANCE";
            break;
        }
        case GL_DEBUG_TYPE_OTHER:
        {
            szType = "OTHER";
            break;
        }
        default:
        {
            szType = "";
            break;
        }
    }

    const char* szSeverity;

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
        {
            szSeverity = "HIGH";
            break;
        }
        case GL_DEBUG_SEVERITY_MEDIUM:
        {
            szSeverity = "MEDIUM";
            break;
        }
        case GL_DEBUG_SEVERITY_LOW:
        {
            szSeverity = "LOW";
            break;
        }
        case GL_DEBUG_SEVERITY_NOTIFICATION:
        {
            #if NS_MINIMUM_LOG_LEVEL != NS_LOG_LEVEL_TRACE
                return;
            #else
                szSeverity = "NOTIFICATION";
                break;
            #endif
        }
        default:
        {
            szSeverity = "";
            break;
        }
    }

    GLsizei n = length == 0 ? (GLsizei)strlen(message) : length;
    if (n > 0)
    {
        n = message[n - 1] == '\n' ? n - 1 : n;
        NS_LOG_DEBUG("%.*s (%s|%s|%s)", n, message, szSource, szType, szSeverity);
    }
}
#endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
void GLRenderContext::FindExtensions()
{
    uint32_t glVersion = GLVersion();

    struct Extension
    {
        enum Enum
        {
            ARB_timer_query,
            EXT_timer_query,
            EXT_disjoint_timer_query,
            ARB_debug_output,
            KHR_debug,

            Count
        };
    
        const char* name;
        bool supported;
    };

    Extension extensions[Extension::Count] =
    {
        { "GL_ARB_timer_query", false},
        { "GL_EXT_timer_query", false},
        { "GL_EXT_disjoint_timer_query", false},
        { "GL_ARB_debug_output", false},
        { "GL_KHR_debug", false},
    };

    if (glVersion >= NS_GL_VER(3,0))
    {
        GLint numExtensions = 0;
        V(glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions));

        for (GLint i = 0; i < numExtensions; i++)
        {
            const char* glExtension = (const char*)glGetStringi(GL_EXTENSIONS, i);

            for (uint32_t j = 0; j < Extension::Count; j++)
            {
                if (String::Equals(glExtension, extensions[j].name))
                {
                    extensions[j].supported = true;
                    break;
                }
            }
        }
    }
    else
    {
        const char* glExtensions = (const char*)glGetString(GL_EXTENSIONS);

        if (glExtensions != 0)
        {
            for (uint32_t i = 0; i < Extension::Count; i++)
            {
                if (String::FindFirst(glExtensions, extensions[i].name) != -1 )
                {
                    extensions[i].supported = true;
                }
            }
        }
    }

#if NS_RENDERER_OPENGL
    if (glVersion >= NS_GL_VER(3,3) || extensions[Extension::ARB_timer_query].supported)
#else
    if (extensions[Extension::ARB_timer_query].supported)
#endif
    {
        #if GL_VERSION_3_3 || GL_ARB_timer_query
            GenQueries_ = glGenQueries;
            DeleteQueries_ = glDeleteQueries;
            BeginQuery_ = glBeginQuery;
            EndQuery_ = glEndQuery;
            GetQueryObjectui64v_ = glGetQueryObjectui64v;
        #else
            NS_ASSERT_UNREACHABLE;
        #endif
    }
    else if (extensions[Extension::EXT_timer_query].supported)
    {
        #if GL_EXT_timer_query
            GenQueries_ = glGenQueries;
            DeleteQueries_ = glDeleteQueries;
            BeginQuery_ = glBeginQuery;
            EndQuery_ = glEndQuery;
            GetQueryObjectui64v_ = glGetQueryObjectui64vEXT;
        #else
            NS_ASSERT_UNREACHABLE;
        #endif
    }
    else if (extensions[Extension::EXT_disjoint_timer_query].supported)
    {
        #if GL_EXT_disjoint_timer_query
            GenQueries_ = glGenQueriesEXT;
            DeleteQueries_ = glDeleteQueriesEXT;
            BeginQuery_ = glBeginQueryEXT;
            EndQuery_ = glEndQueryEXT;
            GetQueryObjectui64v_ = glGetQueryObjectui64vEXT;
        #else
            NS_ASSERT_UNREACHABLE;
        #endif
    }
    else
    {
        GetQueryObjectui64v_ = 0;
    }

    // DebugOutput
#if !NS_RENDERER_USE_EMS && defined(NS_DEBUG)
#if (GL_VERSION_4_3 || GL_ES_VERSION_3_2 || GL_KHR_debug || GL_ARB_debug_output)

    void (GL_APIENTRYP DebugMessageControl_)(GLenum, GLenum, GLenum, GLsizei, const GLuint*,
        GLboolean) = 0;
    void (GL_APIENTRYP DebugMessageCallback_)(GLDEBUGPROC, const void*) = 0;

#if NS_RENDERER_OPENGL
    if (glVersion >= NS_GL_VER(4,3) || extensions[Extension::KHR_debug].supported)
#else
    if (glVersion >= NS_GL_VER(3,2))
#endif
    {
        #if GL_VERSION_4_3 || GL_ES_VERSION_3_2 || GL_KHR_debug
            DebugMessageControl_ = glDebugMessageControl;
            DebugMessageCallback_ = glDebugMessageCallback;
        #else
            NS_ASSERT_UNREACHABLE;
        #endif
    }
#if NS_RENDERER_OPENGL_ES
    else if (extensions[Extension::KHR_debug].supported)
    {
        #if GL_KHR_debug
            DebugMessageControl_ = glDebugMessageControlKHR;
            DebugMessageCallback_ = glDebugMessageCallbackKHR;
        #else
            NS_ASSERT_UNREACHABLE;
        #endif
    }
#endif

    if (DebugMessageControl_ != 0)
    {
        V(DebugMessageControl_(GL_DONT_CARE, GL_DEBUG_TYPE_MARKER, GL_DONT_CARE, 0, 0, GL_FALSE));
        V(DebugMessageControl_(GL_DONT_CARE, GL_DEBUG_TYPE_PUSH_GROUP, GL_DONT_CARE, 0, 0, GL_FALSE));
        V(DebugMessageControl_(GL_DONT_CARE, GL_DEBUG_TYPE_POP_GROUP, GL_DONT_CARE, 0, 0, GL_FALSE));
        V(DebugMessageCallback_((GLDEBUGPROC)DebugMessageCallback, 0));
        V(glEnable(GL_DEBUG_OUTPUT));
        V(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
    }
    else if (extensions[Extension::ARB_debug_output].supported)
    {
        #if GL_ARB_debug_output
            V(glDebugMessageCallbackARB((GLDEBUGPROCARB)DebugMessageCallback, 0));
            V(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB));
        #else
            NS_ASSERT_UNREACHABLE;
        #endif
    }
#endif
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GLRenderContext::HaveQueries() const
{
    return GetQueryObjectui64v_ != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(GLRenderContext)
{
    NsMeta<TypeId>("GLRenderContext");
    NsMeta<Category>("RenderContext");
}
