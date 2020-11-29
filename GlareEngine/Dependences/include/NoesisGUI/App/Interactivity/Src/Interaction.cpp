////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/Interaction.h>
#include <NsApp/BehaviorCollection.h>
#include <NsApp/TriggerCollection.h>
#include <NsGui/FrameworkElement.h>
#include <NsGui/DependencyData.h>
#include <NsCore/TypeId.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
BehaviorCollection* Interaction::GetBehaviors(const DependencyObject* obj)
{
    if (obj == 0)
    {
        NS_ERROR("Can't get Interaction.Behaviors, element is null");
        return 0;
    }

    Ptr<BehaviorCollection> behaviors = obj->GetValue<Ptr<BehaviorCollection>>(BehaviorsProperty);
    if (behaviors == 0)
    {
        behaviors = *new BehaviorCollection();
        DependencyObject* associatedObject = const_cast<DependencyObject*>(obj);
        associatedObject->SetValue<Ptr<BehaviorCollection>>(BehaviorsProperty, behaviors);
    }

    return behaviors;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerCollection* Interaction::GetTriggers(const DependencyObject* obj)
{
    if (obj == 0)
    {
        NS_ERROR("Can't get Interaction.Triggers, element is null");
        return 0;
    }

    Ptr<TriggerCollection> triggers = obj->GetValue<Ptr<TriggerCollection>>(TriggersProperty);
    if (triggers == 0)
    {
        triggers = *new TriggerCollection();
        DependencyObject* associatedObject = const_cast<DependencyObject*>(obj);
        associatedObject->SetValue<Ptr<TriggerCollection>>(TriggersProperty, triggers);
    }

    return triggers;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Interaction::OnBehaviorsChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    BehaviorCollection* oldBehaviors = *static_cast<const Ptr<BehaviorCollection>*>(e.oldValue);
    if (oldBehaviors != 0)
    {
        oldBehaviors->Detach();
    }

    BehaviorCollection* newBehaviors = *static_cast<const Ptr<BehaviorCollection>*>(e.newValue);
    if (newBehaviors != 0)
    {
        NS_ASSERT(newBehaviors->GetAssociatedObject() == 0);
        newBehaviors->Attach(d);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Interaction::OnTriggersChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    TriggerCollection* oldTriggers = *static_cast<const Ptr<TriggerCollection>*>(e.oldValue);
    if (oldTriggers != 0)
    {
        oldTriggers->Detach();
    }

    TriggerCollection* newTriggers = *static_cast<const Ptr<TriggerCollection>*>(e.newValue);
    if (newTriggers != 0)
    {
        NS_ASSERT(newTriggers->GetAssociatedObject() == 0);
        newTriggers->Attach(d);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(Interaction)
{
    NsMeta<TypeId>("NoesisApp.Interaction");
    
    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<BehaviorCollection>>(BehaviorsProperty, "Behaviors",
        PropertyMetadata::Create(Ptr<BehaviorCollection>(), &Interaction::OnBehaviorsChanged));
    data->RegisterProperty<Ptr<TriggerCollection>>(TriggersProperty, "Triggers",
        PropertyMetadata::Create(Ptr<TriggerCollection>(), &Interaction::OnTriggersChanged));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* Interaction::BehaviorsProperty;
const DependencyProperty* Interaction::TriggersProperty;
