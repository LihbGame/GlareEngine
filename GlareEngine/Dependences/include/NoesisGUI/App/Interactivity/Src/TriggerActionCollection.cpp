////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/TriggerActionCollection.h>
#include <NsCore/TypeId.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerActionCollection::TriggerActionCollection()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NoesisApp::TriggerActionCollection::~TriggerActionCollection()
{
    if (GetAssociatedObject() != 0)
    {
        int numActions = Count();
        for (int i = 0; i < numActions; ++i)
        {
            Get(i)->Detach();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::TriggerActionCollection> NoesisApp::TriggerActionCollection::Clone() const
{
    return StaticPtrCast<TriggerActionCollection>(Freezable::Clone());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<NoesisApp::TriggerActionCollection> NoesisApp::TriggerActionCollection::CloneCurrentValue() const
{
    return StaticPtrCast<TriggerActionCollection>(Freezable::CloneCurrentValue());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> NoesisApp::TriggerActionCollection::CreateInstanceCore() const
{
    return *new TriggerActionCollection();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerActionCollection::OnAttached()
{
    DependencyObject* associatedObject = GetAssociatedObject();

    int numActions = Count();
    for (int i = 0; i < numActions; ++i)
    {
        Get(i)->Attach(associatedObject);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerActionCollection::OnDetaching()
{
    int numActions = Count();
    for (int i = 0; i < numActions; ++i)
    {
        Get(i)->Detach();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerActionCollection::ItemAdded(TriggerAction* item)
{
    DependencyObject* associatedObject = GetAssociatedObject();
    if (associatedObject != 0)
    {
        item->Attach(associatedObject);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::TriggerActionCollection::ItemRemoved(TriggerAction* item)
{
    DependencyObject* associatedObject = GetAssociatedObject();
    if (associatedObject != 0)
    {
        item->Detach();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(NoesisApp::TriggerActionCollection)
{
    NsMeta<TypeId>("NoesisApp.TriggerActionCollection");
}
