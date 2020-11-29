////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/StyleInteraction.h>
#include <NsApp/Interaction.h>
#include <NsApp/BehaviorCollection.h>
#include <NsApp/TriggerCollection.h>
#include <NsGui/DependencyData.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
void StyleInteraction::OnBehaviorsChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    BehaviorCollection* behaviors = Interaction::GetBehaviors(d);
    StyleBehaviorCollection* styleBehaviors = static_cast<const Ptr<StyleBehaviorCollection>*>(
        e.newValue)->GetPtr();
    int numBehaviors = styleBehaviors != nullptr ? styleBehaviors->Count() : 0;
    for (int i = 0; i < numBehaviors; ++i)
    {
        // we clone the original behavior to attach a new instance to the styled element
        behaviors->Add(styleBehaviors->Get(i)->Clone());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StyleInteraction::OnTriggersChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    TriggerCollection* triggers = Interaction::GetTriggers(d);
    StyleTriggerCollection* styleTriggers = static_cast<const Ptr<StyleTriggerCollection>*>(
        e.newValue)->GetPtr();
    int numTriggers = styleTriggers != nullptr ? styleTriggers->Count() : 0;
    for (int i = 0; i < numTriggers; ++i)
    {
        // we clone the original trigger to attach a new instance to the styled element
        triggers->Add(styleTriggers->Get(i)->Clone());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(StyleInteraction)
{
    NsMeta<TypeId>("NoesisGUIExtensions.StyleInteraction");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<StyleBehaviorCollection>>(BehaviorsProperty, "Behaviors",
        PropertyMetadata::Create(Ptr<StyleBehaviorCollection>(),
            &StyleInteraction::OnBehaviorsChanged));
    data->RegisterProperty<Ptr<StyleTriggerCollection>>(TriggersProperty, "Triggers",
        PropertyMetadata::Create(Ptr<StyleTriggerCollection>(),
            &StyleInteraction::OnTriggersChanged));
}

NS_IMPLEMENT_REFLECTION(StyleBehaviorCollection)
{
    using namespace Noesis;

    NsMeta<TypeId>("NoesisGUIExtensions.StyleBehaviorCollection");
}

NS_IMPLEMENT_REFLECTION(StyleTriggerCollection)
{
    using namespace Noesis;

    NsMeta<TypeId>("NoesisGUIExtensions.StyleTriggerCollection");
}

const Noesis::DependencyProperty* StyleInteraction::BehaviorsProperty;
const Noesis::DependencyProperty* StyleInteraction::TriggersProperty;
