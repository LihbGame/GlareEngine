////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include "DataBindingHelper.h"

#include <NsGui/BindingOperations.h>
#include <NsGui/BindingExpression.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
void DataBindingHelper::EnsureBindingValue(const DependencyObject* target,
    const DependencyProperty* dp)
{
    BindingExpression* binding = BindingOperations::GetBindingExpression(target, dp);

    if (binding != 0)
    {
        binding->UpdateTarget();
    }
}
