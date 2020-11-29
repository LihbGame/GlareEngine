////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/GoToStateAction.h>
#include <NsGui/DependencyData.h>
#include <NsGui/VisualStateManager.h>
#include <NsGui/VisualStateGroup.h>
#include <NsGui/UICollection.h>
#include <NsGui/UserControl.h>
#include <NsGui/ContentPresenter.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionHelper.h>


using namespace NoesisApp;
using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
GoToStateAction::GoToStateAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
GoToStateAction::~GoToStateAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* GoToStateAction::GetStateName() const
{
    return GetValue<NsString>(StateNameProperty).c_str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GoToStateAction::SetStateName(const char* name)
{
    SetValue<NsString>(StateNameProperty, name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GoToStateAction::GetUseTransitions() const
{
    return GetValue<bool>(UseTransitionsProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GoToStateAction::SetUseTransitions(bool useTransitions)
{
    SetValue<bool>(UseTransitionsProperty, useTransitions);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> GoToStateAction::CreateInstanceCore() const
{
    return *new GoToStateAction();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GoToStateAction::Invoke(BaseComponent*)
{
    if (GetAssociatedObject() != 0 && mStateTarget != 0)
    {
        const char* stateName = GetStateName();
        bool useTransitions = GetUseTransitions();

        if (!String::IsNullOrEmpty(stateName))
        {
            NsSymbol stateId(stateName);
            Control* control = DynamicCast<Control*>(mStateTarget);
            if (control != 0)
            {
                VisualStateManager::GoToState(control, stateId, useTransitions);
            }
            else
            {
                VisualStateManager::GoToElementState(mStateTarget, stateId, useTransitions);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void GoToStateAction::OnTargetChanged(FrameworkElement*, FrameworkElement*)
{
    if (GetTargetObject() == 0 && String::IsNullOrEmpty(GetTargetName()))
    {
        mStateTarget = FindStateGroup(DynamicCast<FrameworkElement*>(GetAssociatedObject()));
    }
    else
    {
        mStateTarget = GetTarget();
    }
}

namespace
{
////////////////////////////////////////////////////////////////////////////////////////////////////
bool HasStateGroup(FrameworkElement* element)
{
    if (element != 0)
    {
        VisualStateGroupCollection* groups = VisualStateManager::GetVisualStateGroups(element);
        return groups != 0 && groups->Count() > 0;
    }
    else
    {
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ShouldWalkTree(FrameworkElement* element)
{
    if (element == 0)
    {
        return false;
    }
    if (DynamicCast<UserControl*>(element) != 0)
    {
        return false;
    }
    if (element->GetParent() == 0)
    {
        FrameworkElement* p = element->GetTemplatedParent();
        if (p == 0 || (DynamicCast<Control*>(p) == 0 && DynamicCast<ContentPresenter*>(p) == 0))
        {
            return false;
        }
    }
    return true;
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
FrameworkElement* GoToStateAction::FindStateGroup(FrameworkElement* context)
{
    if (context != 0)
    {
        FrameworkElement* current = context;
        FrameworkElement* parent = context->GetParent();

        while (!HasStateGroup(current) && ShouldWalkTree(parent))
        {
            current = parent;
            parent = parent->GetParent();
        }

        if (HasStateGroup(current))
        {
            FrameworkElement* templatedParent = current->GetTemplatedParent();
            if (templatedParent != 0 && DynamicCast<Control*>(templatedParent) != 0)
            {
                return templatedParent;
            }
            if (parent != 0 && DynamicCast<UserControl*>(parent) != 0)
            {
                return parent;
            }

            return current;
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(GoToStateAction)
{
    NsMeta<TypeId>("NoesisApp.GoToStateAction");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<NsString>(StateNameProperty, "StateName",
        PropertyMetadata::Create(NsString()));
    data->RegisterProperty<bool>(UseTransitionsProperty, "UseTransitions",
        PropertyMetadata::Create(true));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* GoToStateAction::StateNameProperty;
const DependencyProperty* GoToStateAction::UseTransitionsProperty;
