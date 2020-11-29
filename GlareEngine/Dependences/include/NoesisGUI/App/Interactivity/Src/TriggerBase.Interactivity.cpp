////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/TriggerBase.h>
#include <NsApp/TriggerActionCollection.h>
#include <NsGui/DependencyData.h>
#include <NsGui/PropertyMetadata.h>
#include <NsGui/ContentPropertyMetaData.h>
#include <NsCore/TypeId.h>
#include <NsCore/ReflectionImplement.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerBase::TriggerBase(const TypeClass* associatedType):
    AttachableObject(associatedType)
{
    Ptr<TriggerActionCollection> actions = *new TriggerActionCollection();

    SetReadOnlyProperty<Ptr<TriggerActionCollection>>(ActionsProperty, actions);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerBase::~TriggerBase()
{
    if (GetAssociatedObject() != 0)
    {
        GetActions()->Detach();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerActionCollection* NoesisApp::TriggerBase::GetActions() const
{
    return GetValue<Ptr<TriggerActionCollection>>(ActionsProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DelegateEvent_<PreviewInvokeEventHandler> NoesisApp::TriggerBase::PreviewInvoke()
{
    return mPreviewInvoke;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::TriggerBase> NoesisApp::TriggerBase::Clone() const
{
    return StaticPtrCast<TriggerBase>(Freezable::Clone());

}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::TriggerBase> NoesisApp::TriggerBase::CloneCurrentValue() const
{
    return StaticPtrCast<TriggerBase>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerBase::InvokeActions(BaseComponent* parameter)
{
    PreviewInvokeEventArgs args; args.cancelling = false;
    mPreviewInvoke(this, args);

    if (!args.cancelling)
    {
        TriggerActionCollection* actions = GetActions();
        int numActions = actions->Count();
        for (int i = 0; i < numActions; ++i)
        {
            actions->Get(i)->CallInvoke(parameter);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerBase::OnInit()
{
    ParentClass::OnInit();
    InitComponent(GetActions(), true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerBase::CloneCommonCore(const Freezable* source)
{
    ParentClass::CloneCommonCore(source);

    TriggerActionCollection* thisActions = GetActions();

    TriggerBase* trigger = (TriggerBase*)source;
    TriggerActionCollection* actions = trigger->GetActions();
    int numActions = actions->Count();
    for (int i = 0; i < numActions; ++i)
    {
        Ptr<TriggerAction> action = actions->Get(i)->Clone();
        thisActions->Add(action);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerBase::OnAttached()
{
    GetActions()->Attach(GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerBase::OnDetaching()
{
    GetActions()->Detach();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(NoesisApp::TriggerBase)
{
    NsMeta<TypeId>("NoesisApp.TriggerBase");
    NsMeta<ContentPropertyMetaData>("Actions");

    NsEvent("PreviewInvoke", &NoesisApp::TriggerBase::mPreviewInvoke);

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterPropertyRO<Ptr<TriggerActionCollection>>(ActionsProperty, "Actions",
        PropertyMetadata::Create(Ptr<TriggerActionCollection>()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* NoesisApp::TriggerBase::ActionsProperty;
