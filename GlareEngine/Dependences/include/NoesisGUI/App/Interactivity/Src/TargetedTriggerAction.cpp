////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/TargetedTriggerAction.h>
#include <NsGui/DependencyData.h>
#include <NsGui/FrameworkElement.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsCore/String.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
TargetedTriggerAction::TargetedTriggerAction(const TypeClass* targetType):
    TriggerAction(TypeOf<DependencyObject>()), mTargetType(targetType), mTarget(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
TargetedTriggerAction::~TargetedTriggerAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* TargetedTriggerAction::GetTargetObject() const
{
    return GetValue<Ptr<BaseComponent>>(TargetObjectProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::SetTargetObject(BaseComponent* target)
{
    SetValue<Ptr<BaseComponent>>(TargetObjectProperty, target);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* TargetedTriggerAction::GetTargetName() const
{
    return GetValue<NsString>(TargetNameProperty).c_str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::SetTargetName(const char* name)
{
    SetValue<NsString>(TargetNameProperty, name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<TargetedTriggerAction> TargetedTriggerAction::Clone() const
{
    return StaticPtrCast<TargetedTriggerAction>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<TargetedTriggerAction> TargetedTriggerAction::CloneCurrentValue() const
{
    return StaticPtrCast<TargetedTriggerAction>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* TargetedTriggerAction::GetTarget() const
{
    return mTarget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::OnTargetChangedImpl(BaseComponent*, BaseComponent*)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::OnAttached()
{
    ParentClass::OnAttached();

    UpdateTarget(GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::OnDetaching()
{
    UpdateTarget(0);

    ParentClass::OnDetaching();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::UpdateTarget(DependencyObject* associatedObject)
{
    BaseComponent* oldTarget = mTarget;
    BaseComponent* newTarget = associatedObject;

    if (associatedObject != 0)
    {
        BaseComponent* targetObject = GetTargetObject();
        const char* targetName = GetTargetName();

        if (targetObject != 0)
        {
            newTarget = targetObject;
        }
        else if (!String::IsNullOrEmpty(targetName))
        {
            FrameworkElement* element = DynamicCast<FrameworkElement*>(associatedObject);
            newTarget = element != 0 ? element->FindName(targetName) : 0;
        }
    }

    if (oldTarget != newTarget)
    {
        if (newTarget == 0 || mTargetType->IsAssignableFrom(newTarget->GetClassType()))
        {
            UnregisterTarget();

            mTarget = newTarget;

            RegisterTarget();

            if (GetAssociatedObject() != 0)
            {
                OnTargetChangedImpl(oldTarget, newTarget);
            }
        }
        else
        {
            NS_ERROR("%s invalid target type '%s'", GetClassType()->GetName(),
                newTarget->GetClassType()->GetName());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::RegisterTarget()
{
    if (mTarget != 0)
    {
        DependencyObject* dob = DynamicCast<DependencyObject*>(mTarget);
        if (dob != 0)
        {
            dob->Destroyed() += MakeDelegate(this, &TargetedTriggerAction::OnTargetDestroyed);
        }
        else
        {
            mTarget->AddReference();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::UnregisterTarget()
{
    if (mTarget != 0)
    {
        DependencyObject* dob = DynamicCast<DependencyObject*>(mTarget);
        if (dob != 0)
        {
            dob->Destroyed() -= MakeDelegate(this, &TargetedTriggerAction::OnTargetDestroyed);
        }
        else
        {
            mTarget->Release();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::OnTargetDestroyed(DependencyObject*)
{
    UpdateTarget(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::OnTargetObjectChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    TargetedTriggerAction* action = static_cast<TargetedTriggerAction*>(d);
    action->UpdateTarget(action->GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TargetedTriggerAction::OnTargetNameChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    TargetedTriggerAction* action = static_cast<TargetedTriggerAction*>(d);
    action->UpdateTarget(action->GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(TargetedTriggerAction)
{
    NsMeta<TypeId>("NoesisApp.TargetedTriggerAction");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<BaseComponent>>(TargetObjectProperty, "TargetObject",
        PropertyMetadata::Create(Ptr<BaseComponent>(),
            PropertyChangedCallback(OnTargetObjectChanged)));
    data->RegisterProperty<NsString>(TargetNameProperty, "TargetName",
        PropertyMetadata::Create(NsString(),
            PropertyChangedCallback(OnTargetNameChanged)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* TargetedTriggerAction::TargetObjectProperty;
const DependencyProperty* TargetedTriggerAction::TargetNameProperty;
