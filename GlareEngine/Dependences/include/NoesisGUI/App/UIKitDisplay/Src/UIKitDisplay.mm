////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "UIKitDisplay.h"
#include "ViewController.h"
#include "DisplayView.h"

#include <NsCore/ReflectionImplement.h>
#include <NsCore/Category.h>
#include <NsGui/IntegrationAPI.h>
#include <NsApp/DisplayLauncher.h>
#include <NsApp/CommandLine.h>
#include <UIKit/UIKit.h>


using namespace Noesis;
using namespace NoesisApp;


@interface UISubclassedWindow : UIWindow
@end

@implementation UISubclassedWindow
{
@public
    UIKitDisplay* display;
}

- (instancetype)initWithFrame:(CGRect)frame display:(UIKitDisplay*)display_
{
    self = [super initWithFrame:frame];
    display = display_;
    return self;
}

-(void)activate:(NSNotification*)note
{
    if (display->mDisplayLink)
    {
        NSRunLoop* loop = NSRunLoop.currentRunLoop;
        [display->mDisplayLink addToRunLoop:loop forMode:NSDefaultRunLoopMode];
    }

    display->OnActivated();
}

-(void)deactivate:(NSNotification*)note
{
    if (display->mDisplayLink)
    {
        NSRunLoop* loop = NSRunLoop.currentRunLoop;
        [display->mDisplayLink removeFromRunLoop:loop forMode:NSDefaultRunLoopMode];
    }

    display->OnDeactivated();
}

-(void)render
{
    display->OnRender();
}

@end

////////////////////////////////////////////////////////////////////////////////////////////////////
UIKitDisplay::UIKitDisplay(): mDisplayLink(0)
{
    @autoreleasepool
    {
        CGRect bounds = [[UIScreen mainScreen] bounds];
        mWindow = [[UISubclassedWindow alloc] initWithFrame:bounds display:this];
        mWindow.autoresizesSubviews = YES;
	    mWindow.rootViewController = [[[ViewController alloc] initWithDisplay:this] autorelease];
	    [mWindow setBackgroundColor:[UIColor blackColor]];

        [[NSNotificationCenter defaultCenter] addObserver:mWindow selector:@selector(activate:)
            name:UIApplicationDidBecomeActiveNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:mWindow selector:@selector(deactivate:)
            name:UIApplicationWillResignActiveNotification object:nil];
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UIKitDisplay::~UIKitDisplay()
{
    [mDisplayLink invalidate];
    [mDisplayLink release];

    [[NSNotificationCenter defaultCenter] removeObserver:mWindow
        name:UIApplicationDidBecomeActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:mWindow
        name:UIApplicationWillResignActiveNotification object:nil];

    [mWindow release];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::Show()
{
    mSizeChanged(this, GetClientWidth(), GetClientHeight());
    mLocationChanged(this, 0, 0);
    [mWindow makeKeyAndVisible];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::EnterMessageLoop(bool /*runInBackground*/)
{
    mDisplayLink = [CADisplayLink displayLinkWithTarget:mWindow selector:@selector(render)];
    [mDisplayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] run];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OpenSoftwareKeyboard(Noesis::UIElement* focused)
{
    UIView* view = mWindow.rootViewController.view;
    [view becomeFirstResponder];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::CloseSoftwareKeyboard()
{
    UIView* view = mWindow.rootViewController.view;
    [view resignFirstResponder];
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void* UIKitDisplay::GetNativeHandle() const
{
    return mWindow.rootViewController.view;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t UIKitDisplay::GetClientWidth() const
{
    CGFloat scale = UIScreen.mainScreen.scale;
    return  (uint32_t)(mWindow.bounds.size.width * scale);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t UIKitDisplay::GetClientHeight() const
{
    CGFloat scale = UIScreen.mainScreen.scale;
    return  (uint32_t)(mWindow.bounds.size.height * scale);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnActivated()
{
    mActivated(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnDeactivated()
{
    mDeactivated(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnRender()
{
    mRender(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnSizeChanged(CGFloat width, CGFloat height)
{
    mSizeChanged(this, (uint32_t)width, (uint32_t)height);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnChar(uint32_t c)
{
    mChar(this, c);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnDeleteBackward()
{
    mKeyDown(this, Key::Key_Back);
    mChar(this,'\b');
    mKeyUp(this, Key::Key_Back);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnTouchDown(CGFloat x, CGFloat y, uint64_t id)
{
    mTouchDown(this, (int)x, (int)y, id);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnTouchMove(CGFloat x, CGFloat y, uint64_t id)
{
    mTouchMove(this, (int)x, (int)y, id);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UIKitDisplay::OnTouchUp(CGFloat x, CGFloat y, uint64_t id)
{
    mTouchUp(this, (int)x, (int)y, id);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMPLEMENT_REFLECTION(UIKitDisplay)
{
    NsMeta<TypeId>("UIKitDisplay");
    NsMeta<Category>("Display");
}

