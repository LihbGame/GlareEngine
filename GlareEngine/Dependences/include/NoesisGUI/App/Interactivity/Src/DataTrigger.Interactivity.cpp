////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/DataTrigger.h>
#include <NsGui/Binding.h>
#include <NsGui/UIElementData.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionImplementEnum.h>
#include <NsCore/TypeId.h>
#include <NsCore/TypeConverter.h>

#include "ComparisonLogic.h"
#include "DataBindingHelper.h"


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::DataTrigger::DataTrigger(): mSourceType(0),
    mBinding(DependencyProperty::GetUnsetValue()), mValue(DependencyProperty::GetUnsetValue()),
    mComparison(ComparisonConditionType_Equal)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::DataTrigger::~DataTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* NoesisApp::DataTrigger::GetValue() const
{
    return DependencyObject::GetValue<Ptr<BaseComponent>>(ValueProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::DataTrigger::SetValue(BaseComponent* value)
{
    DependencyObject::SetValue<Ptr<BaseComponent>>(ValueProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ComparisonConditionType NoesisApp::DataTrigger::GetComparison() const
{
    return DependencyObject::GetValue<ComparisonConditionType>(ComparisonProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::DataTrigger::SetComparison(ComparisonConditionType comparison)
{
    DependencyObject::SetValue<ComparisonConditionType>(ComparisonProperty, comparison);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::DataTrigger> NoesisApp::DataTrigger::Clone() const
{
    return StaticPtrCast<DataTrigger>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::DataTrigger> NoesisApp::DataTrigger::CloneCurrentValue() const
{
    return StaticPtrCast<DataTrigger>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> NoesisApp::DataTrigger::CreateInstanceCore() const
{
    return *new DataTrigger();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::DataTrigger::EvaluateBindingChange()
{
    EnsureBindingValues();

    bool sourceChanged = UpdateSourceType();
    bool valueChanged = UpdateTriggerValue();
    bool comparisonChanged = UpdateComparison();

    if (sourceChanged || valueChanged || comparisonChanged)
    {
        if (Compare())
        {
            InvokeActions(0);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::DataTrigger::EnsureBindingValues()
{
    DataBindingHelper::EnsureBindingValue(this, BindingProperty);
    DataBindingHelper::EnsureBindingValue(this, ComparisonProperty);
    DataBindingHelper::EnsureBindingValue(this, ValueProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NoesisApp::DataTrigger::UpdateSourceType()
{
    BaseComponent* binding = GetBinding();
    const Type* type = binding != 0 ? ComparisonLogic::ExtractBoxedType(binding) : 0;

    if (mSourceType != type)
    {
        mSourceType = type;
        mConverter.Reset();
        mComparator.Reset();

        if (mSourceType != 0)
        {
            if (TypeConverter::HasConverter(mSourceType))
            {
                mConverter = TypeConverter::Create(mSourceType);
            }

            mComparator = ComparisonLogic::Create(mSourceType);
        }
    }

    if (!BaseObject::Equals(mBinding, binding))
    {
        mBinding.Reset(binding);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NoesisApp::DataTrigger::UpdateTriggerValue()
{
    Ptr<BaseComponent> value(GetValue());
    const Type* type = value != 0 ? ComparisonLogic::ExtractBoxedType(value) : 0;

    if (mSourceType != 0 && !mSourceType->IsAssignableFrom(type))
    {
        if (mConverter != 0 && value != 0)
        {
            mConverter->TryConvertFrom(value, value);
        }
    }

    if (!BaseObject::Equals(mValue, value))
    {
        mValue.Reset(value);
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NoesisApp::DataTrigger::UpdateComparison()
{
    ComparisonConditionType comparison = GetComparison();

    if (mComparison != comparison)
    {
        mComparison = comparison;
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NoesisApp::DataTrigger::Compare()
{
    if (GetAssociatedObject() != 0)
    {
        if (mComparison == ComparisonConditionType_Equal)
        {
            return BaseObject::Equals(mBinding, mValue);
        }
        else if (mComparison == ComparisonConditionType_NotEqual)
        {
            return !BaseObject::Equals(mBinding, mValue);
        }
        else
        {
            if (mBinding == 0 || mValue == 0 || mComparator == 0)
            {
                return false;
            }

            return mComparator->Evaluate(mBinding, mComparison, mValue);
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::DataTrigger::OnValueChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    DataTrigger* trigger = (DataTrigger*)d;
    trigger->EvaluateTriggerChange();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::DataTrigger::OnComparisonChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    DataTrigger* trigger = (DataTrigger*)d;
    trigger->EvaluateTriggerChange();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(NoesisApp::DataTrigger)
{
    NsMeta<TypeId>("NoesisApp.DataTrigger");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<BaseComponent>>(ValueProperty, "Value",
        PropertyMetadata::Create(Ptr<BaseComponent>(),
            PropertyChangedCallback(OnValueChanged)));
    data->RegisterProperty<ComparisonConditionType>(ComparisonProperty, "Comparison",
        PropertyMetadata::Create(ComparisonConditionType_Equal,
            PropertyChangedCallback(OnComparisonChanged)));
}

NS_IMPLEMENT_REFLECTION_ENUM(ComparisonConditionType)
{
    NsMeta<TypeId>("NoesisApp.ComparisonConditionType");

    NsVal("Equal", ComparisonConditionType_Equal);
    NsVal("NotEqual", ComparisonConditionType_NotEqual);
    NsVal("LessThan", ComparisonConditionType_LessThan);
    NsVal("LessThanOrEqual", ComparisonConditionType_LessThanOrEqual);
    NsVal("GreaterThan", ComparisonConditionType_GreaterThan);
    NsVal("GreaterThanOrEqual", ComparisonConditionType_GreaterThanOrEqual);
}

const DependencyProperty* NoesisApp::DataTrigger::ValueProperty;
const DependencyProperty* NoesisApp::DataTrigger::ComparisonProperty;
