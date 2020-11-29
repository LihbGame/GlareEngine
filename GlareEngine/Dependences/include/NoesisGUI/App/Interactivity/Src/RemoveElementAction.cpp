////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/RemoveElementAction.h>
#include <NsGui/Panel.h>
#include <NsGui/ContentControl.h>
#include <NsGui/ItemsControl.h>
#include <NsGui/Decorator.h>
#include <NsGui/UIElementCollection.h>
#include <NsGui/ItemCollection.h>
#include <NsCore/DynamicCast.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionHelper.h>


using namespace NoesisApp;
using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
RemoveElementAction::RemoveElementAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
RemoveElementAction::~RemoveElementAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> RemoveElementAction::CreateInstanceCore() const
{
    return *new RemoveElementAction();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RemoveElementAction::Invoke(BaseComponent*)
{
    FrameworkElement* target = GetTarget();
    if (GetAssociatedObject() != 0 && target != 0)
    {
        FrameworkElement* parent = target->GetParent();

        Panel* panel = DynamicCast<Panel*>(parent);
        if (panel != 0)
        {
            panel->GetChildren()->Remove(target);
            return;
        }

        ItemsControl* itemsControl = DynamicCast<ItemsControl*>(parent);
        if (itemsControl != 0)
        {
            itemsControl->GetItems()->Remove(target);
            return;
        }

        ContentControl* contentControl = DynamicCast<ContentControl*>(parent);
        if (contentControl != 0)
        {
            if (contentControl->GetContent() == target)
            {
                contentControl->SetContent(static_cast<BaseComponent*>(0));
            }
            return;
        }

        Decorator* decorator = DynamicCast<Decorator*>(parent);
        if (decorator != 0)
        {
            if (decorator->GetChild() == target)
            {
                decorator->SetChild(0);
            }
            return;
        }

        if (parent != 0)
        {
            NS_ERROR("RemoveElementAction: Unsupported parent type '%s' trying to remove '%s'",
                parent->GetClassType()->GetName(), target->GetClassType()->GetName());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(RemoveElementAction)
{
    NsMeta<TypeId>("NoesisApp.RemoveElementAction");
}
