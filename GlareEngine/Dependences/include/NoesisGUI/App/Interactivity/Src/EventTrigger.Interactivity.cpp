////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/EventTrigger.h>
#include <NsGui/UIElementData.h>
#include <NsGui/FrameworkElement.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsCore/String.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::EventTrigger::EventTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::EventTrigger::EventTrigger(const char* eventName)
{
    ForceCreateDependencyProperties();
    SetEventName(eventName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::EventTrigger::~EventTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* NoesisApp::EventTrigger::GetEventName() const
{
    return GetValue<NsString>(EventNameProperty).c_str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::EventTrigger::SetEventName(const char* name)
{
    SetValue<NsString>(EventNameProperty, name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::EventTrigger> NoesisApp::EventTrigger::Clone() const
{
    return StaticPtrCast<EventTrigger>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::EventTrigger> NoesisApp::EventTrigger::CloneCurrentValue() const
{
    return StaticPtrCast<EventTrigger>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> NoesisApp::EventTrigger::CreateInstanceCore() const
{
    return *new EventTrigger();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::EventTrigger::StaticOnEventNameChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    const NsString& oldName = *static_cast<const NsString*>(e.oldValue);
    const NsString& newName = *static_cast<const NsString*>(e.newValue);

    EventTrigger* trigger = static_cast<EventTrigger*>(d);
    trigger->OnEventNameChanged(oldName.c_str(), newName.c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(NoesisApp::EventTrigger)
{
    NsMeta<TypeId>("NoesisApp.EventTrigger");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<NsString>(EventNameProperty, "EventName",
        PropertyMetadata::Create(NsString("Loaded"),
            PropertyChangedCallback(StaticOnEventNameChanged)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* NoesisApp::EventTrigger::EventNameProperty;
