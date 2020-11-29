
////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "TextInputClient.h"
#include "AppKitDisplay.h"
#include "WindowDelegate.h"

#include <NsGui/TextBox.h>
#include <NsGui/IView.h>
#include <NsGui/ScrollViewer.h>
#include <NsGui/Decorator.h>
#include <NsMath/Matrix.h>


using namespace Noesis;
using namespace NoesisApp;


@implementation TextInputClient

Noesis::TextBox* mControl;
NoesisApp::AppKitDisplay* mDisplay;

- (instancetype)initWithDisplay:(AppKitDisplay*)display
{
    [super initWithFrame:NSMakeRect(0.0f, 0.0f, 0.0f, 0.0f)];
    mDisplay = display;
    return self;
}

- (void)setControl:(Noesis::TextBox*)control
{
    mControl = control;
}

- (void)removeMarkedText
{
    NSRange r = [self markedRange];
    if (r.location != NSNotFound)
    {
        mControl->Select((int32_t)r.location, (int32_t)r.length);
        mControl->SetSelectedText("");
        mControl->ClearCompositionUnderlines();
    }
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
    actualRange:(NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    Noesis::Rect r = mControl->GetRangeBounds((uint32_t)range.location, (uint32_t)NSMaxRange(range));
    Visual* visual = mControl->GetTextView();
    Matrix4f m = visual->TransformToAncestor(mControl->GetView()->GetContent());
    r.Transform(m);

    CGFloat height = mDisplay->mWindow.contentView.bounds.size.height;
    NSRect r_ = NSMakeRect(r.x, height - (r.y + r.height), r.width, r.height);
    return [mDisplay->mWindow convertRectToScreen:r_];
}

- (BOOL)hasMarkedText
{
    return mControl->GetNumCompositionUnderlines() > 0;
}

- (NSRange)markedRange
{
    if (mControl->GetNumCompositionUnderlines() > 0)
    {
        CompositionUnderline u = mControl->GetCompositionUnderline(0);
        return NSMakeRange(u.start, u.end - u.start);
    }

    return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
    return NSMakeRange(mControl->GetSelectionStart(), mControl->GetSelectionLength());
}

- (void)insertText_:(id)string replacementRange:(NSRange)replacementRange
{
    if (replacementRange.location != NSNotFound)
    {
        mControl->Select((int32_t)replacementRange.location, (int32_t)replacementRange.length);
    }

    if ([string isKindOfClass: [NSAttributedString class]])
    {
        mControl->SetSelectedText([[string string] UTF8String]);
        mControl->UpdateLayout();
    }
    else
    {
        mControl->SetSelectedText([string UTF8String]);
        mControl->UpdateLayout();
    }

    mControl->SetCaretIndex(mControl->GetSelectionStart() + mControl->GetSelectionLength());
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    [self removeMarkedText];
    [self insertText_:string replacementRange:replacementRange];
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    NSRange r;

    // If there is no marked text, the current selection is replaced. If there is no selection,
    // the string is inserted at the insertion point
    if ([self hasMarkedText])
    {
        r = [self markedRange];
    }
    else
    {
        r = [self selectedRange];
    }

    if (replacementRange.location != NSNotFound)
    {
        r.location += replacementRange.location;
        r.length = replacementRange.length;
    }

    mControl->ClearCompositionUnderlines();
    [self insertText_:string replacementRange:r];

    NSUInteger len = [string length];
    if (len > 0)
    {
        CompositionUnderline u =
        {
            (uint32_t)r.location,
            (uint32_t)(r.location + len),
            CompositionLineStyle_Solid
        };

        mControl->AddCompositionUnderline(u);
    }

    mControl->Select((int32_t)(r.location + selectedRange.location), (int32_t)selectedRange.length);
}

- (void)unmarkText
{
    mControl->ClearCompositionUnderlines();
}

- (NSArray*)validAttributesForMarkedText
{
    return [NSArray array];
}

- (void)doCommandBySelector:(SEL)selector
{
    // Redirect special key events (like arrows, backspace) to the view
    [super keyDown:[NSApp currentEvent]];
}

- (void)keyDown:(NSEvent*)event
{
    [self interpretKeyEvents:[NSArray arrayWithObject:event]];
}

@end
