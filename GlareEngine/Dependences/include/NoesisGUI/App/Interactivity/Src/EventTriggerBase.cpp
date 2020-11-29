////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/EventTriggerBase.h>
#include <NsGui/UIElementData.h>
#include <NsGui/FrameworkElement.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsCore/String.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
EventTriggerBase::EventTriggerBase(const TypeClass* sourceType):
    TriggerBase(TypeOf<DependencyObject>()), mSourceType(sourceType), mSource(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
EventTriggerBase::~EventTriggerBase()
{
    if (GetAssociatedObject() != 0)
    {
        UnregisterEvent();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* EventTriggerBase::GetSourceObject() const
{
    return GetValue<Ptr<BaseComponent>>(SourceObjectProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::SetSourceObject(BaseComponent* source)
{
    SetValue<Ptr<BaseComponent>>(SourceObjectProperty, source);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* EventTriggerBase::GetSourceName() const
{
    return GetValue<NsString>(SourceNameProperty).c_str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::SetSourceName(const char* name)
{
    SetValue<NsString>(SourceNameProperty, name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<EventTriggerBase> EventTriggerBase::Clone() const
{
    return StaticPtrCast<EventTriggerBase>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<EventTriggerBase> EventTriggerBase::CloneCurrentValue() const
{
    return StaticPtrCast<EventTriggerBase>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BaseComponent* EventTriggerBase::GetSource() const
{
    return mSource;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnSourceChangedImpl(BaseComponent*, BaseComponent* newSource)
{
    UnregisterEvent();
    RegisterEvent(newSource, GetEventName());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnEventNameChanged(const char*, const char* newName)
{
    UnregisterEvent();
    RegisterEvent(mSource, newName);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnEvent()
{
    InvokeActions(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnAttached()
{
    ParentClass::OnAttached();

    UpdateSource(GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnDetaching()
{
    UpdateSource(0);

    ParentClass::OnDetaching();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::UpdateSource(DependencyObject* associatedObject)
{
    BaseComponent* oldSource = mSource;
    BaseComponent* newSource = associatedObject;

    if (associatedObject != 0)
    {
        BaseComponent* sourceObject = GetSourceObject();
        const char* sourceName = GetSourceName();

        if (sourceObject != 0)
        {
            newSource = sourceObject;
        }
        else if (!String::IsNullOrEmpty(sourceName))
        {
            FrameworkElement* element = DynamicCast<FrameworkElement*>(associatedObject);
            BaseComponent* found = element != 0 ? element->FindName(sourceName) : 0;
            if (found != 0)
            {
                newSource = found;
            }
        }
    }

    if (oldSource != newSource)
    {
        if (newSource == 0 || mSourceType->IsAssignableFrom(newSource->GetClassType()))
        {
            mSource = newSource;

            if (GetAssociatedObject() != 0)
            {
                OnSourceChangedImpl(oldSource, newSource);
            }
        }
        else
        {
            NS_ERROR("%s invalid source type '%s'", GetClassType()->GetName(),
                newSource->GetClassType()->GetName());
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::RegisterEvent(BaseComponent* source, const char* eventName)
{
    if (source != 0 && !String::IsNullOrEmpty(eventName))
    {
        NsSymbol eventId = NsSymbol(eventName);
        const TypeClass* type = source->GetClassType();

        UIElement* element = DynamicCast<UIElement*>(source);
        if (element != 0)
        {
            const RoutedEvent* routedEvent = FindRoutedEvent(eventId, type);
            if (routedEvent != 0)
            {
                element->AddHandler(routedEvent, MakeDelegate(this,
                    &EventTriggerBase::OnRoutedEvent));

                mUnregisterEvent = [this, element, routedEvent]()
                {
                    element->RemoveHandler(routedEvent, MakeDelegate(this,
                        &EventTriggerBase::OnRoutedEvent));
                };

                return;
            }
        }

        const TypeProperty* event = FindEvent(eventId, type);
        if (event != 0)
        {
            EventHandler& handler = *(EventHandler*)event->GetContent(source);
            handler += MakeDelegate(this, &EventTriggerBase::OnDelegateEvent);

            mUnregisterEvent = [this, &handler]()
            {
                handler -= MakeDelegate(this, &EventTriggerBase::OnDelegateEvent);
            };
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::UnregisterEvent()
{
    mUnregisterEvent();
    mUnregisterEvent = UnregisterEventDelegate();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const RoutedEvent* EventTriggerBase::FindRoutedEvent(NsSymbol eventName, const TypeClass* type)
{
    const TypeClass* elementType = TypeOf<UIElement>();
    while (type != 0)
    {
        const UIElementData* metadata = type->GetMetaData().Find<UIElementData>();

        if (metadata != 0)
        {
            const RoutedEvent* event = metadata->FindEvent(eventName);
            if (event != 0)
            {
                return event;
            }
        }

        if (type == elementType)
        {
            break;
        }

        type = type->GetBase();
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnRoutedEvent(BaseComponent*, const RoutedEventArgs&)
{
    OnEvent();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const TypeProperty* EventTriggerBase::FindEvent(NsSymbol eventName, const TypeClass* type)
{
    while (type != 0)
    {
        const TypeProperty* event = type->FindEvent(eventName);
        if (event != 0)
        {
            return event;
        }

        type = type->GetBase();
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnDelegateEvent(BaseComponent*, const EventArgs&)
{
    OnEvent();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnSourceObjectChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    EventTriggerBase* trigger = static_cast<EventTriggerBase*>(d);
    trigger->UpdateSource(trigger->GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void EventTriggerBase::OnSourceNameChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    EventTriggerBase* trigger = static_cast<EventTriggerBase*>(d);
    trigger->UpdateSource(trigger->GetAssociatedObject());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(EventTriggerBase)
{
    NsMeta<TypeId>("NoesisApp.EventTriggerBase");

    DependencyData* data = NsMeta<DependencyData>(TypeOf<SelfClass>());
    data->RegisterProperty<Ptr<BaseComponent>>(SourceObjectProperty, "SourceObject",
        PropertyMetadata::Create(Ptr<BaseComponent>(),
            PropertyChangedCallback(OnSourceObjectChanged)));
    data->RegisterProperty<NsString>(SourceNameProperty, "SourceName",
        PropertyMetadata::Create(NsString(),
            PropertyChangedCallback(OnSourceNameChanged)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* EventTriggerBase::SourceObjectProperty;
const DependencyProperty* EventTriggerBase::SourceNameProperty;
