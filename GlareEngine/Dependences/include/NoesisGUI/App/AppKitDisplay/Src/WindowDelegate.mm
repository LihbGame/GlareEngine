////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "WindowDelegate.h"
#include "AppKitDisplay.h"
#include "KeyCodes.h"

#include <AppKit/AppKit.h>
#include <NsGui/InputEnums.h>


using namespace Noesis;
using namespace NoesisApp;


namespace
{

enum KeyModifier
{
    KeyModifier_Shift = 1,
    KeyModifier_Alt = 2,
    KeyModifier_Control = 4
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////
@implementation WindowDelegate

uint32_t _modifiers = 0;
AppKitDisplay* _display;

- (void)setDisplay:(NoesisApp::AppKitDisplay*)display
{
    _display = display;
}

- (void)windowWillClose:(NSNotification *)notification
{
    _display->mClosed = true;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    _display->OnActivated();
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    _display->OnDeactivated();
}

- (void)windowDidResize:(NSNotification *)notification
{
    _display->OnSizeChanged(self.frame.size.width, self.frame.size.height);
}

-(void)windowDidMove:(NSNotification *)notification
{
    _display->OnLocationChanged(self.frame.origin.x, self.frame.origin.y);
}

- (NSPoint)getMousePos:(NSEvent*)event
{
    CGFloat height = self.contentView.bounds.size.height;
    return { event.locationInWindow.x, height - event.locationInWindow.y };
}

- (uint32_t)getModifiers:(NSEvent*)event
{
    uint32_t modifiers = 0;

    if (event.modifierFlags & NSShiftKeyMask) modifiers |= KeyModifier_Shift;
    if (event.modifierFlags & NSAlternateKeyMask) modifiers |= KeyModifier_Alt;
    if (event.modifierFlags & NSCommandKeyMask) modifiers |= KeyModifier_Control;
    if (event.modifierFlags & NSControlKeyMask) modifiers |= KeyModifier_Control;

    return modifiers;
}

- (void)updateModifiers:(NSEvent*)event
{
    uint32_t modifiers = [self getModifiers:event];
    uint32_t delta = modifiers ^ _modifiers;

    if (delta & KeyModifier_Control)
    {
        if (modifiers & KeyModifier_Control)
        {
            _display->OnKeyDown(VK_Control);
        }
        else
        {
            _display->OnKeyUp(VK_Control);
        }
    }

    if (delta & KeyModifier_Alt)
    {
        if (modifiers & KeyModifier_Alt)
        {
            _display->OnKeyDown(VK_Option);
        }
        else
        {
            _display->OnKeyUp(VK_Option);
        }
    }

    if (delta & KeyModifier_Shift)
    {
        if (modifiers & KeyModifier_Shift)
        {
            _display->OnKeyDown(VK_Shift);
        }
        else
        {
            _display->OnKeyUp(VK_Shift);
        }
    }

    _modifiers = modifiers;
}

- (void)mouseDown:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];

    if (event.clickCount == 2)
    {
        _display->OnMouseDoubleClick(p.x, p.y, MouseButton_Left);
    }
    else
    {
        _display->OnMouseButtonDown(p.x, p.y, MouseButton_Left);
    }
}

- (void)mouseUp:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseButtonUp(p.x, p.y, MouseButton_Left);
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];

    if (event.clickCount == 2)
    {
        _display->OnMouseDoubleClick(p.x, p.y, MouseButton_Right);
    }
    else
    {
        _display->OnMouseButtonDown(p.x, p.y, MouseButton_Right);
    }
}

- (void)rightMouseUp:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseButtonUp(p.x, p.y, MouseButton_Right);
}

- (void)otherMouseDown:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];

    if (event.clickCount == 2)
    {
        _display->OnMouseDoubleClick(p.x, p.y, MouseButton_Middle);
    }
    else
    {
        _display->OnMouseButtonDown(p.x, p.y, MouseButton_Middle);
    }
}

- (void)otherMouseUp:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseButtonUp(p.x, p.y, MouseButton_Middle);
}

- (void)mouseMoved:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseMove(p.x, p.y);
}

- (void)mouseDragged:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseMove(p.x, p.y);
}

- (void)scrollWheel:(NSEvent *)event;
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseWheel(p.x, p.y, event.deltaY);
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseMove(p.x, p.y);
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self updateModifiers:event];
    NSPoint p = [self getMousePos:event];
    _display->OnMouseMove(p.x, p.y);
}

- (void)keyDown:(NSEvent *)event
{
    [self updateModifiers:event];
    _display->OnKeyDown(event.keyCode);
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
}

- (void)keyUp:(NSEvent *)event
{
    [self updateModifiers:event];
    _display->OnKeyUp(event.keyCode);
}

-(void)insertText:(id)string
{
    for (int i = 0; i < [string length]; i++)
    {
        unichar ch = [string characterAtIndex:i];
        _display->OnChar(ch);
    }
}

- (void)doCommandBySelector:(SEL)aSelector
{
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender
{
    return NSDragOperationEvery;
}

- (BOOL)performDragOperation:(id < NSDraggingInfo >)sender
{
    NSArray *files = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    for (NSString *file in files)
    {
        _display->OnFileDropped(file.UTF8String);
    }

    return YES;
}

@end
