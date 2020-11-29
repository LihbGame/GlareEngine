////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/ConditionalExpression.h>
#include <NsApp/ComparisonCondition.h>
#include <NsGui/DependencyData.h>
#include <NsGui/PropertyMetadata.h>
#include <NsGui/ContentPropertyMetaData.h>
#include <NsGui/FreezableCollection.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>
#include <NsCore/TypeId.h>


using namespace NoesisApp;
using namespace Noesis;


////////////////////////////////////////////////////////////////////////////////////////////////////
ConditionalExpression::ConditionalExpression()
{
    ForceCreateDependencyProperties();
    SetValue<Ptr<ConditionCollection>>(ConditionsProperty, MakePtr<ConditionCollection>());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ConditionalExpression::~ConditionalExpression()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ForwardChaining ConditionalExpression::GetForwardChaining() const
{
    return GetValue<ForwardChaining>(ForwardChainingProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ConditionalExpression::SetForwardChaining(ForwardChaining value)
{
    SetValue<ForwardChaining>(ForwardChainingProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::ConditionCollection* ConditionalExpression::GetConditions() const
{
    return GetValue<Ptr<ConditionCollection>>(ConditionsProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> ConditionalExpression::CreateInstanceCore() const
{
    return *new ConditionalExpression();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ConditionalExpression::Evaluate() const
{
    bool flag = false;

    ForwardChaining chaining = GetForwardChaining();
    ConditionCollection* conditions = GetConditions();
    int numConditions = conditions != 0 ? conditions->Count() : 0;
    for (int i = 0; i < numConditions; ++i)
    {
        ComparisonCondition* condition = conditions->Get(i);

        flag = condition->Evaluate();

        if (!flag && chaining == ForwardChaining_And)
        {
            return flag;
        }
        if (flag && chaining == ForwardChaining_Or)
        {
            return flag;
        }
    }

    return flag;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(ConditionalExpression)
{
    NsMeta<TypeId>("NoesisApp.ConditionalExpression");
    NsMeta<ContentPropertyMetaData>("Conditions");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<ForwardChaining>(ForwardChainingProperty, "ForwardChaining",
        PropertyMetadata::Create(ForwardChaining_And));
    data->RegisterProperty<Ptr<ConditionCollection>>(ConditionsProperty, "Conditions",
        PropertyMetadata::Create(Ptr<ConditionCollection>()));
}


NS_IMPLEMENT_REFLECTION_ENUM(ForwardChaining)
{
    NsMeta<TypeId>("NoesisApp.ForwardChaining");

    NsVal("And", ForwardChaining_And);
    NsVal("Or", ForwardChaining_Or);
}

const DependencyProperty* ConditionalExpression::ConditionsProperty;
const DependencyProperty* ConditionalExpression::ForwardChainingProperty;
