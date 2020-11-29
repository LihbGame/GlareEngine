////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/StoryboardCompletedTrigger.h>
#include <NsGui/Storyboard.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
StoryboardCompletedTrigger::StoryboardCompletedTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
StoryboardCompletedTrigger::~StoryboardCompletedTrigger()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<StoryboardCompletedTrigger> StoryboardCompletedTrigger::Clone() const
{
    return StaticPtrCast<StoryboardCompletedTrigger>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<StoryboardCompletedTrigger> StoryboardCompletedTrigger::CloneCurrentValue() const
{
    return StaticPtrCast<StoryboardCompletedTrigger>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> StoryboardCompletedTrigger::CreateInstanceCore() const
{
    return *new StoryboardCompletedTrigger();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoryboardCompletedTrigger::OnDetaching()
{
    Storyboard* storyboard = GetStoryboard();
    if (storyboard != 0)
    {
        storyboard->Completed() -= MakeDelegate(this,
            &StoryboardCompletedTrigger::OnStoryboardCompleted);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoryboardCompletedTrigger::OnStoryboardChanged(const DependencyPropertyChangedEventArgs& e)
{
    Storyboard* oldStoryboard = static_cast<const Ptr<Storyboard>*>(e.oldValue)->GetPtr();
    Storyboard* newStoryboard = static_cast<const Ptr<Storyboard>*>(e.newValue)->GetPtr();

    if (oldStoryboard != 0)
    {
        oldStoryboard->Completed() -= MakeDelegate(this,
            &StoryboardCompletedTrigger::OnStoryboardCompleted);
    }
    if (newStoryboard != 0)
    {
        newStoryboard->Completed() += MakeDelegate(this,
            &StoryboardCompletedTrigger::OnStoryboardCompleted);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoryboardCompletedTrigger::OnStoryboardCompleted(BaseComponent*, const TimelineEventArgs&)
{
    InvokeActions(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(StoryboardCompletedTrigger)
{
    NsMeta<TypeId>("NoesisApp.StoryboardCompletedTrigger");
}
