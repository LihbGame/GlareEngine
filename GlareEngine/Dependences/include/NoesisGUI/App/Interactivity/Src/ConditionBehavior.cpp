////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/ConditionBehavior.h>
#include <NsApp/ConditionalExpression.h>
#include <NsGui/DependencyData.h>
#include <NsGui/PropertyMetadata.h>
#include <NsGui/ContentPropertyMetaData.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>


using namespace NoesisApp;
using namespace Noesis;


////////////////////////////////////////////////////////////////////////////////////////////////////
ConditionBehavior::ConditionBehavior()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ConditionBehavior::~ConditionBehavior()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ConditionalExpression* ConditionBehavior::GetCondition() const
{
    return GetValue<Ptr<ConditionalExpression>>(ConditionProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConditionBehavior::SetCondition(ConditionalExpression* value)
{
    SetValue<Ptr<ConditionalExpression>>(ConditionProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConditionBehavior::OnAttached()
{
    TriggerBase* trigger = GetAssociatedObject();
    trigger->PreviewInvoke() += MakeDelegate(this, &ConditionBehavior::OnPreviewInvoke);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConditionBehavior::OnDetaching()
{
    TriggerBase* trigger = GetAssociatedObject();
    trigger->PreviewInvoke() -= MakeDelegate(this, &ConditionBehavior::OnPreviewInvoke);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> ConditionBehavior::CreateInstanceCore() const
{
    return *new ConditionBehavior();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConditionBehavior::OnPreviewInvoke(BaseComponent*, const PreviewInvokeEventArgs& e)
{
    ConditionalExpression* condition = GetCondition();
    if (condition != 0)
    {
        e.cancelling = !condition->Evaluate();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(ConditionBehavior)
{
    NsMeta<TypeId>("NoesisApp.ConditionBehavior");
    NsMeta<ContentPropertyMetaData>("Condition");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<ConditionalExpression>>(ConditionProperty, "Condition",
        PropertyMetadata::Create(Ptr<ConditionalExpression>()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* ConditionBehavior::ConditionProperty;
