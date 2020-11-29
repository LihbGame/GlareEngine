////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/ComparisonCondition.h>
#include <NsGui/DependencyData.h>
#include <NsGui/PropertyMetadata.h>
#include <NsGui/FreezableCollection.h>
#include <NsGui/BindingOperations.h>
#include <NsGui/BindingExpression.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsCore/TypeConverter.h>

#include "ComparisonLogic.h"
#include "DataBindingHelper.h"


using namespace NoesisApp;
using namespace Noesis;


////////////////////////////////////////////////////////////////////////////////////////////////////
ComparisonCondition::ComparisonCondition(): mSourceType(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ComparisonCondition::~ComparisonCondition()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* ComparisonCondition::GetLeftOperand() const
{
    return GetValue<Ptr<BaseComponent>>(LeftOperandProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ComparisonCondition::SetLeftOperand(BaseComponent* value)
{
    SetValue<Ptr<BaseComponent>>(LeftOperandProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ComparisonConditionType ComparisonCondition::GetOperator() const
{
    return GetValue<ComparisonConditionType>(OperatorProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ComparisonCondition::SetOperator(ComparisonConditionType value)
{
    SetValue<ComparisonConditionType>(OperatorProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* ComparisonCondition::GetRightOperand() const
{
    return GetValue<Ptr<BaseComponent>>(RightOperandProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ComparisonCondition::SetRightOperand(BaseComponent* value)
{
    SetValue<Ptr<BaseComponent>>(RightOperandProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool ComparisonCondition::Evaluate() const
{
    // Make sure all binding values are up to date
    EnsureBindingValues();

    // Ensure both operands have the same type to be compared
    EnsureOperands();

    // Do the comparison
    ComparisonConditionType comparison = GetOperator();
    if (comparison == ComparisonConditionType_Equal)
    {
        return BaseObject::Equals(mLeft, mRight);
    }
    else if (comparison == ComparisonConditionType_NotEqual)
    {
        return !BaseObject::Equals(mLeft, mRight);
    }
    else
    {
        if (mLeft == 0 || mRight == 0 || mComparator == 0)
        {
            return false;
        }

        return mComparator->Evaluate(mLeft, comparison, mRight);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> ComparisonCondition::CreateInstanceCore() const
{
    return *new ComparisonCondition();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ComparisonCondition::EnsureBindingValues() const
{
    DataBindingHelper::EnsureBindingValue(this, LeftOperandProperty);
    DataBindingHelper::EnsureBindingValue(this, OperatorProperty);
    DataBindingHelper::EnsureBindingValue(this, RightOperandProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ComparisonCondition::EnsureOperands() const
{
    // ensure left operand
    Ptr<BaseComponent> left(GetLeftOperand());
    const Type* leftType = left != 0 ? ComparisonLogic::ExtractBoxedType(left) : 0;

    if (mSourceType != leftType)
    {
        mSourceType = leftType;
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

    mLeft.Reset(left);

    // ensure right operand
    Ptr<BaseComponent> right(GetRightOperand());
    const Type* rightType = right != 0 ? ComparisonLogic::ExtractBoxedType(right) : 0;

    if (mSourceType != rightType)
    {
        if (mConverter != 0 && right != 0)
        {
            mConverter->TryConvertFrom(right, right);
        }
    }

    mRight.Reset(right);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(ComparisonCondition)
{
    NsMeta<TypeId>("NoesisApp.ComparisonCondition");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<BaseComponent>>(LeftOperandProperty, "LeftOperand",
        PropertyMetadata::Create(Ptr<BaseComponent>()));
    data->RegisterProperty<ComparisonConditionType>(OperatorProperty, "Operator",
        PropertyMetadata::Create(ComparisonConditionType_Equal));
    data->RegisterProperty<Ptr<BaseComponent>>(RightOperandProperty, "RightOperand",
        PropertyMetadata::Create(Ptr<BaseComponent>()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* ComparisonCondition::LeftOperandProperty;
const DependencyProperty* ComparisonCondition::OperatorProperty;
const DependencyProperty* ComparisonCondition::RightOperandProperty;
