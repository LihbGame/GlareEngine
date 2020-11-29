////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/GamepadTrigger.h>
#include <NsGui/UIElementData.h>
#include <NsGui/Keyboard.h>
#include <NsGui/TypeConverterMetaData.h>
#include <NsGui/InputEnums.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>
#include <NsCore/TypeId.h>
#include <NsCore/String.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
GamepadTrigger::GamepadTrigger(): mSource(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GamepadTrigger::~GamepadTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GamepadButton GamepadTrigger::GetButton() const
{
    return GetValue<GamepadButton>(ButtonProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::SetButton(GamepadButton button)
{
    SetValue<GamepadButton>(ButtonProperty, button);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GamepadTrigger::GetActiveOnFocus() const
{
    return GetValue<bool>(ActiveOnFocusProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::SetActiveOnFocus(bool activeOnFocus)
{
    SetValue<bool>(ActiveOnFocusProperty, activeOnFocus);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GamepadTriggerFiredOn GamepadTrigger::GetFiredOn() const
{
    return GetValue<GamepadTriggerFiredOn>(FiredOnProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::SetFiredOn(GamepadTriggerFiredOn firedOn)
{
    SetValue<GamepadTriggerFiredOn>(FiredOnProperty, firedOn);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<GamepadTrigger> GamepadTrigger::Clone() const
{
    return StaticPtrCast<GamepadTrigger>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<GamepadTrigger> GamepadTrigger::CloneCurrentValue() const
{
    return StaticPtrCast<GamepadTrigger>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> GamepadTrigger::CreateInstanceCore() const
{
    return *new GamepadTrigger();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::OnAttached()
{
    ParentClass::OnAttached();
    RegisterSource();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::OnDetaching()
{
    UnregisterSource();
    ParentClass::OnDetaching();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UIElement* GamepadTrigger::GetRoot(UIElement* current) const
{
    UIElement* root = 0;

    while (current != 0)
    {
        root = current;
        current = current->GetUIParent();
    }

    return root;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::OnButtonPress(BaseComponent*, const KeyEventArgs& e)
{
    if (GetButton() == (GamepadButton)(e.key - Key_GamepadLeft))
    {
        InvokeActions(0);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::RegisterSource()
{
    mSource = GetActiveOnFocus() ? GetAssociatedObject() : GetRoot(GetAssociatedObject());

    if (mSource != 0)
    {
        if (GetFiredOn() == GamepadTriggerFiredOn_ButtonDown)
        {
            mSource->KeyDown() += MakeDelegate(this, &GamepadTrigger::OnButtonPress);
        }
        else
        {
            mSource->KeyUp() += MakeDelegate(this, &GamepadTrigger::OnButtonPress);
        }

        mSource->Destroyed() += MakeDelegate(this, &GamepadTrigger::OnSourceDestroyed);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::UnregisterSource()
{
    if (mSource != 0)
    {
        mSource->Destroyed() -= MakeDelegate(this, &GamepadTrigger::OnSourceDestroyed);

        if (GetFiredOn() == GamepadTriggerFiredOn_ButtonDown)
        {
            mSource->KeyDown() -= MakeDelegate(this, &GamepadTrigger::OnButtonPress);
        }
        else
        {
            mSource->KeyUp() -= MakeDelegate(this, &GamepadTrigger::OnButtonPress);
        }

        mSource = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GamepadTrigger::OnSourceDestroyed(DependencyObject*)
{
    UnregisterSource();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(GamepadTrigger)
{
    NsMeta<TypeId>("NoesisGUIExtensions.GamepadTrigger");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<GamepadButton>(ButtonProperty, "Button",
        PropertyMetadata::Create(GamepadButton_Accept));
    data->RegisterProperty<bool>(ActiveOnFocusProperty, "ActiveOnFocus",
        PropertyMetadata::Create(false));
    data->RegisterProperty<GamepadTriggerFiredOn>(FiredOnProperty, "FiredOn",
        PropertyMetadata::Create(GamepadTriggerFiredOn_ButtonDown));
}

NS_IMPLEMENT_REFLECTION_ENUM(GamepadTriggerFiredOn)
{
    NsMeta<TypeId>("NoesisGUIExtensions.GamepadTriggerFiredOn");

    NsVal("ButtonDown", GamepadTriggerFiredOn_ButtonDown);
    NsVal("ButtonUp", GamepadTriggerFiredOn_ButtonUp);
}

NS_IMPLEMENT_REFLECTION_ENUM(GamepadButton)
{
    NsMeta<TypeId>("NoesisGUIExtensions.GamepadButton");

    NsVal("Left", GamepadButton_Left);
    NsVal("Up", GamepadButton_Up);
    NsVal("Right", GamepadButton_Right);
    NsVal("Down", GamepadButton_Down);
    NsVal("Accept", GamepadButton_Accept);
    NsVal("Cancel", GamepadButton_Cancel);
    NsVal("Menu", GamepadButton_Menu);
    NsVal("View", GamepadButton_View);
    NsVal("PageUp", GamepadButton_PageUp);
    NsVal("PageDown", GamepadButton_PageDown);
    NsVal("PageLeft", GamepadButton_PageLeft);
    NsVal("PageRight", GamepadButton_PageRight);
    NsVal("Context1", GamepadButton_Context1);
    NsVal("Context2", GamepadButton_Context2);
    NsVal("Context3", GamepadButton_Context3);
    NsVal("Context4", GamepadButton_Context4);
}

const DependencyProperty* GamepadTrigger::ButtonProperty;
const DependencyProperty* GamepadTrigger::ActiveOnFocusProperty;
const DependencyProperty* GamepadTrigger::FiredOnProperty;
