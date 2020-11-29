////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/KeyTrigger.h>
#include <NsGui/UIElementData.h>
#include <NsGui/Keyboard.h>
#include <NsGui/TypeConverterMetaData.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>
#include <NsCore/TypeId.h>
#include <NsCore/String.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
KeyTrigger::KeyTrigger(): mSource(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
KeyTrigger::~KeyTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Key KeyTrigger::GetKey() const
{
    return GetValue<Key>(KeyProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::SetKey(Key key)
{
    SetValue<Key>(KeyProperty, key);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ModifierKeys KeyTrigger::GetModifiers() const
{
    return GetValue<ModifierKeys>(ModifiersProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::SetModifiers(ModifierKeys modifiers)
{
    SetValue<ModifierKeys>(ModifiersProperty, modifiers);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool KeyTrigger::GetActiveOnFocus() const
{
    return GetValue<bool>(ActiveOnFocusProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::SetActiveOnFocus(bool activeOnFocus)
{
    SetValue<bool>(ActiveOnFocusProperty, activeOnFocus);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
KeyTriggerFiredOn KeyTrigger::GetFiredOn() const
{
    return GetValue<KeyTriggerFiredOn>(FiredOnProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::SetFiredOn(KeyTriggerFiredOn firedOn)
{
    SetValue<KeyTriggerFiredOn>(FiredOnProperty, firedOn);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<KeyTrigger> KeyTrigger::Clone() const
{
    return StaticPtrCast<KeyTrigger>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<KeyTrigger> KeyTrigger::CloneCurrentValue() const
{
    return StaticPtrCast<KeyTrigger>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> KeyTrigger::CreateInstanceCore() const
{
    return *new KeyTrigger();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::OnAttached()
{
    ParentClass::OnAttached();
    RegisterSource();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::OnDetaching()
{
    UnregisterSource();
    ParentClass::OnDetaching();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
UIElement* KeyTrigger::GetRoot(UIElement* current) const
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
bool KeyTrigger::CheckModifiers() const
{
    UIElement* element = static_cast<UIElement*>(mSource);
    Keyboard* keyboard = element->GetKeyboard();
    return GetModifiers() == keyboard->GetModifiers();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::OnKeyPress(BaseComponent*, const KeyEventArgs& e)
{
    if (GetKey() == e.key && CheckModifiers())
    {
        InvokeActions(0);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::RegisterSource()
{
    mSource = GetActiveOnFocus() ? GetAssociatedObject() : GetRoot(GetAssociatedObject());

    if (mSource != 0)
    {
        if (GetFiredOn() == KeyTriggerFiredOn_KeyDown)
        {
            mSource->KeyDown() += MakeDelegate(this, &KeyTrigger::OnKeyPress);
        }
        else
        {
            mSource->KeyUp() += MakeDelegate(this, &KeyTrigger::OnKeyPress);
        }

        mSource->Destroyed() += MakeDelegate(this, &KeyTrigger::OnSourceDestroyed);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::UnregisterSource()
{
    if (mSource != 0)
    {
        mSource->Destroyed() -= MakeDelegate(this, &KeyTrigger::OnSourceDestroyed);

        if (GetFiredOn() == KeyTriggerFiredOn_KeyDown)
        {
            mSource->KeyDown() -= MakeDelegate(this, &KeyTrigger::OnKeyPress);
        }
        else
        {
            mSource->KeyUp() -= MakeDelegate(this, &KeyTrigger::OnKeyPress);
        }

        mSource = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KeyTrigger::OnSourceDestroyed(DependencyObject*)
{
    UnregisterSource();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(KeyTrigger)
{
    NsMeta<TypeId>("NoesisApp.KeyTrigger");

    NsProp("Key", &KeyTrigger::GetKey, &KeyTrigger::SetKey)
        .Meta<TypeConverterMetaData>("Converter<Key>");
    NsProp("Modifiers", &KeyTrigger::GetModifiers, &KeyTrigger::SetModifiers)
        .Meta<TypeConverterMetaData>("Converter<ModifierKeys>");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Key>(KeyProperty, "Key",
        PropertyMetadata::Create(Key_None));
    data->RegisterProperty<ModifierKeys>(ModifiersProperty, "Modifiers",
        PropertyMetadata::Create(ModifierKeys_None));
    data->RegisterProperty<bool>(ActiveOnFocusProperty, "ActiveOnFocus",
        PropertyMetadata::Create(false));
    data->RegisterProperty<KeyTriggerFiredOn>(FiredOnProperty, "FiredOn",
        PropertyMetadata::Create(KeyTriggerFiredOn_KeyDown));
}

NS_IMPLEMENT_REFLECTION_ENUM(KeyTriggerFiredOn)
{
    NsMeta<TypeId>("NoesisApp.KeyTriggerFiredOn");

    NsVal("KeyDown", KeyTriggerFiredOn_KeyDown);
    NsVal("KeyUp", KeyTriggerFiredOn_KeyUp);
}

const DependencyProperty* KeyTrigger::KeyProperty;
const DependencyProperty* KeyTrigger::ModifiersProperty;
const DependencyProperty* KeyTrigger::ActiveOnFocusProperty;
const DependencyProperty* KeyTrigger::FiredOnProperty;
