////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/ChangePropertyAction.h>
#include <NsGui/DependencyData.h>
#include <NsGui/Duration.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionHelper.h>
#include <NsCore/TypeConverter.h>

#include "ComparisonLogic.h"


using namespace NoesisApp;
using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
ChangePropertyAction::ChangePropertyAction(): mTypeProperty(0), mDependencyProperty(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ChangePropertyAction::~ChangePropertyAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* ChangePropertyAction::GetPropertyName() const
{
    return DependencyObject::GetValue<NsString>(PropertyNameProperty).c_str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::SetPropertyName(const char* name)
{
    DependencyObject::SetValue<NsString>(PropertyNameProperty, name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* ChangePropertyAction::GetValue() const
{
    return DependencyObject::GetValue<Ptr<BaseComponent>>(ValueProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::SetValue(BaseComponent* value)
{
    DependencyObject::SetValue<Ptr<BaseComponent>>(ValueProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const Noesis::Duration& ChangePropertyAction::GetDuration() const
{
    return DependencyObject::GetValue<Noesis::Duration>(DurationProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::SetDuration(const Noesis::Duration& duration)
{
    DependencyObject::SetValue<Noesis::Duration>(DurationProperty, duration);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ChangePropertyAction::GetIncrement() const
{
    return DependencyObject::GetValue<bool>(IncrementProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::SetIncrement(bool increment)
{
    DependencyObject::SetValue<bool>(IncrementProperty, increment);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> ChangePropertyAction::CreateInstanceCore() const
{
    return *new ChangePropertyAction();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::Invoke(BaseComponent*)
{
    BaseComponent* target = GetTarget();
    if (GetAssociatedObject() != 0 && target != 0 && (
        mTypeProperty != 0 || mDependencyProperty != 0))
    {
        const Noesis::Duration& duration = GetDuration();
        if (duration.GetDurationType() == DurationType_TimeSpan)
        {
            NS_ERROR("ChangePropertyAction with Duration animation not implemented");
            SetPropertyValue();
        }
        else if (GetIncrement())
        {
            NS_ERROR("ChangePropertyAction with Increment not implemented");
            SetPropertyValue();
        }
        else
        {
            SetPropertyValue();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::OnTargetChanged(BaseComponent*, BaseComponent*)
{
    if (UpdateProperty())
    {
        UpdateConvertedValue();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const Type* ChangePropertyAction::GetPropertyType() const
{
    const Type* type = 0;
    if (mTypeProperty != 0)
    {
        type = mTypeProperty->GetContentType();
    }
    else if (mDependencyProperty != 0)
    {
        type = mDependencyProperty->GetType();
    }

    return type;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ChangePropertyAction::UpdateProperty()
{
    const Type* oldType = GetPropertyType();

    mTypeProperty = 0;
    mDependencyProperty = 0;

    const Type* newType = 0;
    BaseComponent* target = GetTarget();
    if (target != 0)
    {
        const TypeClass* targetType = target->GetClassType();
        const char* propName = GetPropertyName();
        NsSymbol propId(propName);

        if (propId.IsNull())
        {
            NS_ERROR("Cannot find a property named '%s' on type '%s'",
                propName, targetType->GetName());
            return true;
        }

        mDependencyProperty = FindDependencyProperty(targetType, propId);
        if (mDependencyProperty != 0)
        {
            if (mDependencyProperty->IsReadOnly())
            {
                NS_ERROR("Property '%s.%s' is read-only", targetType->GetName(), propName);
                return true;
            }

            newType = mDependencyProperty->GetType();
        }
        else
        {
            mTypeProperty = Reflection::FindProperty(target->GetClassType(), propId).property;
            if (mTypeProperty != 0)
            {
                if (mTypeProperty->IsReadOnly())
                {
                    NS_ERROR("Property '%s.%s' is read-only", targetType->GetName(), propName);
                    return true;
                }

                newType = mTypeProperty->GetContentType();
            }
            else
            {
                NS_ERROR("Cannot find a property named '%s' on type '%s'",
                    propName, targetType->GetName());
                return true;
            }
        }
    }

    if (oldType != newType)
    {
        mConverter.Reset();

        if (newType != 0 && TypeConverter::HasConverter(newType))
        {
            mConverter = TypeConverter::Create(newType);
        }

        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::UpdateConvertedValue()
{
    Ptr<BaseComponent> value(GetValue());
    const Type* valueType = value != 0 ? ComparisonLogic::ExtractBoxedType(value) : 0;
    const Type* propertyType = GetPropertyType();

    if (propertyType != 0 && !propertyType->IsAssignableFrom(valueType))
    {
        if (mConverter != 0 && value != 0)
        {
            mConverter->TryConvertFrom(value, value);
        }
    }

    mConvertedValue.Reset(value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::SetPropertyValue()
{
    BaseComponent* target = GetTarget();

    if (mTypeProperty != 0)
    {
        mTypeProperty->SetComponent(target, mConvertedValue);
    }
    else
    {
        DependencyObject* d = (DependencyObject*)target;
        d->SetValueObject(mDependencyProperty, mConvertedValue);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::OnPropertyNameChanged(Noesis::DependencyObject* d,
    const Noesis::DependencyPropertyChangedEventArgs&)
{
    ChangePropertyAction* trigger = (ChangePropertyAction*)d;

    if (trigger->UpdateProperty())
    {
        trigger->UpdateConvertedValue();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::OnValueChanged(Noesis::DependencyObject* d,
    const Noesis::DependencyPropertyChangedEventArgs&)
{
    ChangePropertyAction* trigger = (ChangePropertyAction*)d;

    trigger->UpdateProperty();
    trigger->UpdateConvertedValue();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::OnDurationChanged(Noesis::DependencyObject*,
    const Noesis::DependencyPropertyChangedEventArgs&)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ChangePropertyAction::OnIncrementChanged(Noesis::DependencyObject*,
    const Noesis::DependencyPropertyChangedEventArgs&)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(ChangePropertyAction)
{
    NsMeta<TypeId>("NoesisApp.ChangePropertyAction");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<NsString>(PropertyNameProperty, "PropertyName",
        PropertyMetadata::Create(NsString(), PropertyChangedCallback(OnPropertyNameChanged)));
    data->RegisterProperty<Ptr<BaseComponent>>(ValueProperty, "Value",
        PropertyMetadata::Create(Ptr<BaseComponent>(), PropertyChangedCallback(OnValueChanged)));
    data->RegisterProperty<Noesis::Duration>(DurationProperty, "Duration",
        PropertyMetadata::Create(Noesis::Duration(), PropertyChangedCallback(OnDurationChanged)));
    data->RegisterProperty<bool>(IncrementProperty, "Increment",
        PropertyMetadata::Create(false, PropertyChangedCallback(OnIncrementChanged)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* ChangePropertyAction::PropertyNameProperty;
const DependencyProperty* ChangePropertyAction::ValueProperty;
const DependencyProperty* ChangePropertyAction::DurationProperty;
const DependencyProperty* ChangePropertyAction::IncrementProperty;
