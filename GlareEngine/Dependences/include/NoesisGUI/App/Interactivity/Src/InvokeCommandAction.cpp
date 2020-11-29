////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/InvokeCommandAction.h>
#include <NsGui/ICommand.h>
#include <NsGui/DependencyData.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/ReflectionHelper.h>


using namespace NoesisApp;
using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
InvokeCommandAction::InvokeCommandAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
InvokeCommandAction::~InvokeCommandAction()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* InvokeCommandAction::GetCommandName() const
{
    return mCommandName.c_str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InvokeCommandAction::SetCommandName(const char* name)
{
    mCommandName = name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ICommand* InvokeCommandAction::GetCommand() const
{
    return DynamicCast<ICommand*>(GetValue<Ptr<BaseComponent>>(CommandProperty).GetPtr());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InvokeCommandAction::SetCommand(ICommand* command)
{
    SetValue<Ptr<BaseComponent>>(CommandProperty, (BaseComponent*)command->GetBaseObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* InvokeCommandAction::GetCommandParameter() const
{
    return GetValue<Ptr<BaseComponent>>(CommandParameterProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InvokeCommandAction::SetCommandParameter(BaseComponent* parameter)
{
    SetValue<Ptr<BaseComponent>>(CommandProperty, parameter);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> InvokeCommandAction::CreateInstanceCore() const
{
    return *new InvokeCommandAction();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InvokeCommandAction::Invoke(BaseComponent*)
{
    if (GetAssociatedObject() != 0)
    {
        ICommand* command = ResolveCommand();
        BaseComponent* param = GetCommandParameter();
        if (command != 0 && command->CanExecute(param))
        {
            command->Execute(param);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
ICommand* InvokeCommandAction::ResolveCommand() const
{
    ICommand* command = GetCommand();
    if (command != 0)
    {
        return command;
    }

    if (!mCommandName.empty())
    {
        NsSymbol propName(mCommandName);

        DependencyObject* associatedObject = GetAssociatedObject();
        NS_ASSERT(associatedObject != 0);

        const TypeClass* type = associatedObject->GetClassType();
        Reflection::TypeClassProperty p = Reflection::FindProperty(type, propName);
        if (p.property != 0)
        {
            return DynamicCast<ICommand*>(p.property->GetComponent(associatedObject).GetPtr());
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(InvokeCommandAction)
{
    NsMeta<TypeId>("NoesisApp.InvokeCommandAction");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<BaseComponent>>(CommandProperty, "Command",
        PropertyMetadata::Create(Ptr<BaseComponent>()));
    data->RegisterProperty<Ptr<BaseComponent>>(CommandParameterProperty, "CommandParameter",
        PropertyMetadata::Create(Ptr<BaseComponent>()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* InvokeCommandAction::CommandProperty;
const DependencyProperty* InvokeCommandAction::CommandParameterProperty;
