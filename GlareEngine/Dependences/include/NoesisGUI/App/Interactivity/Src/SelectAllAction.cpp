////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/SelectAllAction.h>
#include <NsGui/TextBox.h>
#include <NsGui/PasswordBox.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>


using namespace NoesisApp;
using namespace Noesis;


////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> SelectAllAction::CreateInstanceCore() const
{
    return *new SelectAllAction();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SelectAllAction::Invoke(Noesis::BaseComponent*)
{
    Control* control = GetAssociatedObject();

    TextBox* textBox = DynamicCast<TextBox*>(control);
    if (textBox != 0)
    {
        textBox->SelectAll();
        return;
    }

    PasswordBox* passwordBox = DynamicCast<PasswordBox*>(control);
    if (passwordBox != 0)
    {
        passwordBox->SelectAll();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(SelectAllAction)
{
    NsMeta<TypeId>("NoesisGUIExtensions.SelectAllAction");
}
