////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/ControlStoryboardAction.h>
#include <NsGui/DependencyData.h>
#include <NsGui/Storyboard.h>
#include <NsGui/ResourceDictionary.h>
#include <NsGui/FrameworkElement.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
ControlStoryboardAction::ControlStoryboardAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ControlStoryboardAction::~ControlStoryboardAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ControlStoryboardOption ControlStoryboardAction::GetControlStoryboardOption() const
{
    return GetValue<ControlStoryboardOption>(ControlStoryboardOptionProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ControlStoryboardAction::SetControlStoryboardOption(ControlStoryboardOption option)
{
    SetValue<ControlStoryboardOption>(ControlStoryboardOptionProperty, option);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> ControlStoryboardAction::CreateInstanceCore() const
{
    return *new ControlStoryboardAction;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ControlStoryboardAction::CloneCommonCore(const Freezable* source)
{
    ParentClass::CloneCommonCore(source);

    ControlStoryboardAction* action = (ControlStoryboardAction*)source;
    Storyboard* storyboard = action->GetStoryboard();
    if (DynamicCast<ResourceDictionary*>(storyboard->GetNodeParent()) != 0)
    {
        // use original Storyboard resource, not a clone
        SetStoryboard(storyboard);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ControlStoryboardAction::Invoke(BaseComponent*)
{
    Storyboard* storyboard = GetStoryboard();
    DependencyObject* associatedObject = GetAssociatedObject();
    if (associatedObject != 0 && storyboard != 0)
    {
        FrameworkElement* target = FindTreeElement(storyboard->GetNodeParent());
        if (target != 0)
        {
            switch (GetControlStoryboardOption())
            {
                case ControlStoryboardOption_Play:
                {
                    IUITreeNode* node = DynamicCast<IUITreeNode*>(associatedObject);
                    FrameworkElement* ns = FindTreeElement(node);
                    storyboard->Begin(target, ns, true);
                    break;
                }
                case ControlStoryboardOption_Stop:
                {
                    storyboard->Stop(target);
                    break;
                }
                case ControlStoryboardOption_TogglePlayPause:
                {
                    if (storyboard->IsPaused(target))
                    {
                        storyboard->Resume(target);
                    }
                    else if (storyboard->IsPlaying(target))
                    {
                        storyboard->Pause(target);
                    }
                    else
                    {
                        IUITreeNode* node = DynamicCast<IUITreeNode*>(associatedObject);
                        FrameworkElement* ns = FindTreeElement(node);
                        storyboard->Begin(target, ns, true);
                    }
                    break;
                }
                case ControlStoryboardOption_Pause:
                {
                    storyboard->Pause(target);
                    break;
                }
                case ControlStoryboardOption_Resume:
                {
                    storyboard->Resume(target);
                    break;
                }
                case ControlStoryboardOption_SkipToFill:
                {
                    NS_ERROR("Storyboard.SkipToFill is not implemented");
                    break;
                }
                default: NS_ASSERT_UNREACHABLE;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(ControlStoryboardAction)
{
    NsMeta<TypeId>("NoesisApp.ControlStoryboardAction");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<ControlStoryboardOption>(ControlStoryboardOptionProperty,
        "ControlStoryboardOption", PropertyMetadata::Create(ControlStoryboardOption_Play));
}

NS_IMPLEMENT_REFLECTION_ENUM(ControlStoryboardOption)
{
    NsMeta<TypeId>("NoesisApp.ControlStoryboardOption");

    NsVal("Play", ControlStoryboardOption_Play);
    NsVal("Stop", ControlStoryboardOption_Stop);
    NsVal("TogglePlayPause", ControlStoryboardOption_TogglePlayPause);
    NsVal("Pause", ControlStoryboardOption_Pause);
    NsVal("Resume", ControlStoryboardOption_Resume);
    NsVal("SkipToFill", ControlStoryboardOption_SkipToFill);
}

const DependencyProperty* ControlStoryboardAction::ControlStoryboardOptionProperty;
