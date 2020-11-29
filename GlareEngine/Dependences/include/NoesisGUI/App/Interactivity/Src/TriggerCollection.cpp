////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/TriggerCollection.h>
#include <NsCore/TypeId.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerCollection::TriggerCollection()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerCollection::~TriggerCollection()
{
    if (GetAssociatedObject() != 0)
    {
        int numTriggers = Count();
        for (int i = 0; i < numTriggers; ++i)
        {
            Get(i)->Detach();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::TriggerCollection> NoesisApp::TriggerCollection::Clone() const
{
    return StaticPtrCast<TriggerCollection>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::TriggerCollection> NoesisApp::TriggerCollection::CloneCurrentValue() const
{
    return StaticPtrCast<TriggerCollection>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> NoesisApp::TriggerCollection::CreateInstanceCore() const
{
    return *new TriggerCollection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool NoesisApp::TriggerCollection::FreezeCore(bool)
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerCollection::OnAttached()
{
    DependencyObject* associatedObject = GetAssociatedObject();

    int numTriggers = Count();
    for (int i = 0; i < numTriggers; ++i)
    {
        Get(i)->Attach(associatedObject);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerCollection::OnDetaching()
{
    int numTriggers = Count();
    for (int i = 0; i < numTriggers; ++i)
    {
        Get(i)->Detach();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerCollection::ItemAdded(TriggerBase* item)
{
    DependencyObject* associatedObject = GetAssociatedObject();
    if (associatedObject != 0)
    {
        item->Attach(associatedObject);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerCollection::ItemRemoved(TriggerBase* item)
{
    DependencyObject* associatedObject = GetAssociatedObject();
    if (associatedObject != 0)
    {
        item->Detach();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(NoesisApp::TriggerCollection)
{
    NsMeta<TypeId>("NoesisApp.TriggerCollection");
}
