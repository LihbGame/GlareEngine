////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "ComparisonLogic.h"
#include <NsCore/Ptr.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeConverter.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
template<class T>
struct ComparisonLogicImpl final: public ComparisonLogic
{
    bool Evaluate(BaseComponent* left, ComparisonConditionType comparison,
        BaseComponent* right) const final
    {
        if (!ValidValue(left))
        {
            return false;
        }
        if (!ValidValue(right))
        {
            return false;
        }

        switch (comparison)
        {
            case ComparisonConditionType_Equal:
                return Boxing::Unbox<T>(left) == Boxing::Unbox<T>(right);
            case ComparisonConditionType_NotEqual:
                return Boxing::Unbox<T>(left) != Boxing::Unbox<T>(right);
            case ComparisonConditionType_LessThan:
                return Boxing::Unbox<T>(left) < Boxing::Unbox<T>(right);
            case ComparisonConditionType_LessThanOrEqual:
                return Boxing::Unbox<T>(left) <= Boxing::Unbox<T>(right);
            case ComparisonConditionType_GreaterThan:
                return Boxing::Unbox<T>(left) > Boxing::Unbox<T>(right);
            case ComparisonConditionType_GreaterThanOrEqual:
                return Boxing::Unbox<T>(left) >= Boxing::Unbox<T>(right);
            default:
                NS_ASSERT_UNREACHABLE;
        }
    }

    bool ValidValue(BaseComponent* value) const
    {
        if (Boxing::CanUnbox<T>(value))
        {
            return true;
        }

        if (TypeOf<T>() == TypeOf<int32_t>())
        {
            BoxedValue* boxed = DynamicCast<BoxedValue*>(value);
            if (boxed != 0 && DynamicCast<const TypeEnum*>(boxed->GetValueType()) != 0)
            {
                return true;
            }
        }

        return false;
    }
};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const Type* ComparisonLogic::ExtractBoxedType(BaseComponent* value)
{
    NS_ASSERT(value != 0);

    const Type* type = value->GetClassType();

    if (DynamicCast<const TypeEnum*>(type) == 0)
    {
        BoxedValue* boxedValue = DynamicCast<BoxedValue*>(value);
        if (boxedValue != 0)
        {
            const Type* boxedType = boxedValue->GetValueType();
            if (TypeConverter::HasConverter(boxedType))
            {
                return boxedType;
            }
        }
    }

    return type;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<ComparisonLogic> ComparisonLogic::Create(const Noesis::Type* type)
{
    if (type == TypeOf<uint8_t>())
    {
        return *new ComparisonLogicImpl<uint8_t>();
    }
    else if (type == TypeOf<uint16_t>())
    {
        return *new ComparisonLogicImpl<uint16_t>();
    }
    else if (type == TypeOf<uint32_t>())
    {
        return *new ComparisonLogicImpl<uint32_t>();
    }
    else if (type == TypeOf<uint64_t>())
    {
        return *new ComparisonLogicImpl<uint64_t>();
    }
    else if (type == TypeOf<int8_t>())
    {
        return *new ComparisonLogicImpl<int8_t>();
    }
    else if (type == TypeOf<int16_t>())
    {
        return *new ComparisonLogicImpl<int16_t>();
    }
    else if (type == TypeOf<int32_t>())
    {
        return *new ComparisonLogicImpl<int32_t>();
    }
    else if (type == TypeOf<int64_t>())
    {
        return *new ComparisonLogicImpl<int64_t>();
    }
    else if (type == TypeOf<float>())
    {
        return *new ComparisonLogicImpl<float>();
    }
    else if (type == TypeOf<double>())
    {
        return *new ComparisonLogicImpl<double>();
    }
    else if (type == TypeOf<char>())
    {
        return *new ComparisonLogicImpl<char>();
    }
    else if (type == TypeOf<NsString>())
    {
        return *new ComparisonLogicImpl<NsString>();
    }
    else if (DynamicCast<const TypeEnum*>(type) != 0)
    {
        return *new ComparisonLogicImpl<int32_t>();
    }

    return 0;
}

NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION_(ComparisonLogic)
