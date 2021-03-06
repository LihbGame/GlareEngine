////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/SelectAction.h>
#include <NsGui/Selector.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>


using namespace NoesisApp;
using namespace Noesis;


////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> SelectAction::CreateInstanceCore() const
{
    return *new SelectAction();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SelectAction::Invoke(Noesis::BaseComponent*)
{
    Control* control = GetAssociatedObject();
    if (control != 0)
    {
        Selector::SetIsSelected(control, true);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(SelectAction)
{
    NsMeta<TypeId>("NoesisGUIExtensions.SelectAction");
}
