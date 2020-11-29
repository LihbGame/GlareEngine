////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_TEXTINPUTCLIENT_H__
#define __APP_TEXTINPUTCLIENT_H__


#include <NsCore/Noesis.h>
#include <AppKit/NSTextInputClient.h>
#include <AppKit/NSView.h>


namespace Noesis { class TextBox; }
namespace NoesisApp { class AppKitDisplay; }


////////////////////////////////////////////////////////////////////////////////////////////////////
@interface TextInputClient : NSView<NSTextInputClient>

- (instancetype)initWithDisplay:(NoesisApp::AppKitDisplay*)display;
- (void)setControl:(Noesis::TextBox*)control;

@end


#endif
