////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/MouseDragElementBehavior.h>
#include <NsGui/TranslateTransform.h>
#include <NsGui/VisualTreeHelper.h>
#include <NsGui/UIElementData.h>
#include <NsGui/PropertyMetadata.h>
#include <NsCore/ReflectionImplement.h>
#include <NsCore/TypeId.h>
#include <NsMath/Matrix.h>


using namespace NoesisApp;
using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
MouseDragElementBehavior::MouseDragElementBehavior(): mSettingPosition(false)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
MouseDragElementBehavior::~MouseDragElementBehavior()
{
    FrameworkElement* associatedObject = GetAssociatedObject();
    if (associatedObject != 0)
    {
        associatedObject->MouseLeftButtonDown() -= MakeDelegate(this,
            &MouseDragElementBehavior::OnMouseLeftButtonDown);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float MouseDragElementBehavior::GetX() const
{
    return GetValue<float>(XProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::SetX(float x)
{
    SetValue<float>(XProperty, x);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
float MouseDragElementBehavior::GetY() const
{
    return GetValue<float>(YProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::SetY(float y)
{
    SetValue<float>(YProperty, y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool MouseDragElementBehavior::GetConstrainToParentBounds() const
{
    return GetValue<bool>(ConstrainToParentBoundsProperty);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::SetConstrainToParentBounds(bool value)
{
    SetValue<bool>(ConstrainToParentBoundsProperty, value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DelegateEvent_<MouseEventHandler> MouseDragElementBehavior::DragBegun()
{
    return mDragBegun;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DelegateEvent_<MouseEventHandler> MouseDragElementBehavior::Dragging()
{
    return mDragging;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
DelegateEvent_<MouseEventHandler> MouseDragElementBehavior::DragFinished()
{
    return mDragFinished;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnAttached()
{
    FrameworkElement* associatedObject = GetAssociatedObject();

    mTransform = *new TranslateTransform();
    associatedObject->SetRenderTransform(mTransform);
    associatedObject->MouseLeftButtonDown() += MakeDelegate(this,
        &MouseDragElementBehavior::OnMouseLeftButtonDown);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnDetaching()
{
    FrameworkElement* associatedObject = GetAssociatedObject();

    mTransform = 0;
    associatedObject->SetRenderTransform(0);
    associatedObject->MouseLeftButtonDown() -= MakeDelegate(this,
        &MouseDragElementBehavior::OnMouseLeftButtonDown);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
Ptr<Freezable> MouseDragElementBehavior::CreateInstanceCore() const
{
    return *new MouseDragElementBehavior();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnMouseLeftButtonDown(BaseComponent*, const MouseButtonEventArgs& e)
{
    StartDrag(GetAssociatedObject()->PointFromScreen(e.position));
    mDragBegun(this, e);
    e.handled = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnMouseLeftButtonUp(BaseComponent*, const MouseButtonEventArgs&)
{
    GetAssociatedObject()->ReleaseMouseCapture();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnMouseMove(BaseComponent*, const MouseEventArgs& e)
{
    Drag(GetAssociatedObject()->PointFromScreen(e.position));
    mDragging(this, e);
    e.handled = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnLostMouseCapture(BaseComponent*, const MouseEventArgs& e)
{
    EndDrag();
    mDragFinished(this, e);
    e.handled = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::StartDrag(Noesis::Point relativePosition)
{
    mRelativePosition = relativePosition;

    FrameworkElement* associatedObject = GetAssociatedObject();
    associatedObject->MouseMove() += MakeDelegate(this, &MouseDragElementBehavior::OnMouseMove);
    associatedObject->LostMouseCapture() += MakeDelegate(this,
        &MouseDragElementBehavior::OnLostMouseCapture);
    associatedObject->MouseLeftButtonUp() += MakeDelegate(this,
        &MouseDragElementBehavior::OnMouseLeftButtonUp);

    associatedObject->CaptureMouse();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::Drag(Noesis::Point relativePosition)
{
    Noesis::Point delta = relativePosition - mRelativePosition;
    UpdateTransform(delta.x, delta.y);

    mSettingPosition = true;
    UpdatePosition();
    mSettingPosition = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::EndDrag()
{
    FrameworkElement* associatedObject = GetAssociatedObject();
    associatedObject->MouseMove() -= MakeDelegate(this, &MouseDragElementBehavior::OnMouseMove);
    associatedObject->LostMouseCapture() -= MakeDelegate(this,
        &MouseDragElementBehavior::OnLostMouseCapture);
    associatedObject->MouseLeftButtonUp() -= MakeDelegate(this,
        &MouseDragElementBehavior::OnMouseLeftButtonUp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::UpdateTransform(float x, float y)
{
    if (GetConstrainToParentBounds())
    {
        FrameworkElement* associatedObject = GetAssociatedObject();
        FrameworkElement* parent = associatedObject->GetParent();
        if (parent != 0)
        {
            Noesis::Point minXY(0.0f, 0.0f);
            Noesis::Point maxXY(parent->GetActualWidth(), parent->GetActualHeight());

            Matrix4f m = associatedObject->TransformToAncestor(parent);
            Noesis::Point p0(m[3][0] + x, m[3][1] + y);
            Noesis::Point p1(
                associatedObject->GetActualWidth(),
                associatedObject->GetActualHeight());
            p1 += p0;

            if (p1.x > maxXY.x)
            {
                float dif = p1.x - maxXY.x;
                x -= dif;
                p0.x -= dif;
                p1.x -= dif;
            }
            if (p0.x < minXY.x)
            {
                float dif = minXY.x - p0.x;
                x += dif;
            }
            if (p1.y > maxXY.y)
            {
                float dif = p1.y - maxXY.y;
                y -= dif;
                p0.y -= dif;
                p1.y -= dif;
            }
            if (p0.y < minXY.y)
            {
                float dif = minXY.y - p0.y;
                y += dif;
            }
        }
    }

    mTransform->SetX(mTransform->GetX() + x);
    mTransform->SetY(mTransform->GetY() + y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::UpdatePosition(float x, float y)
{
    FrameworkElement* associatedObject = GetAssociatedObject();
    if (!mSettingPosition && associatedObject != 0 && associatedObject->IsConnectedToView())
    {
        Noesis::Point pos(IsNaN(x) ? 0.0f : x, IsNaN(y) ? 0.0f : y);
        Noesis::Point delta = associatedObject->PointFromScreen(pos);
        UpdateTransform(delta.x, delta.y);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::UpdatePosition()
{
    FrameworkElement* associatedObject = GetAssociatedObject();
    Visual* root = VisualTreeHelper::GetRoot(associatedObject);
    Matrix4f m = associatedObject->TransformToAncestor(root);
    SetX(m[3][0]);
    SetY(m[3][1]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnXChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    MouseDragElementBehavior* behavior = (MouseDragElementBehavior*)d;
    behavior->UpdatePosition(*static_cast<const float*>(e.newValue), behavior->GetY());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnYChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs& e)
{
    MouseDragElementBehavior* behavior = (MouseDragElementBehavior*)d;
    behavior->UpdatePosition(behavior->GetX(), *static_cast<const float*>(e.newValue));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MouseDragElementBehavior::OnConstrainToParentBoundsChanged(DependencyObject* d,
    const DependencyPropertyChangedEventArgs&)
{
    MouseDragElementBehavior* behavior = (MouseDragElementBehavior*)d;
    behavior->UpdatePosition(behavior->GetX(), behavior->GetY());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
NS_BEGIN_COLD_REGION

NS_IMPLEMENT_REFLECTION(MouseDragElementBehavior)
{
    NsMeta<TypeId>("NoesisApp.MouseDragElementBehavior");

    NsEvent("DragBegun", &MouseDragElementBehavior::mDragBegun);
    NsEvent("Dragging", &MouseDragElementBehavior::mDragging);
    NsEvent("DragFinished", &MouseDragElementBehavior::mDragFinished);

    float nan = std::numeric_limits<float>::quiet_NaN();

    UIElementData* data = NsMeta<UIElementData>(TypeOf<SelfClass>());
    data->RegisterProperty<float>(XProperty, "X",
        PropertyMetadata::Create(nan, PropertyChangedCallback(OnXChanged)));
    data->RegisterProperty<float>(YProperty, "Y",
        PropertyMetadata::Create(nan, PropertyChangedCallback(OnYChanged)));
    data->RegisterProperty<bool>(ConstrainToParentBoundsProperty, "ConstrainToParentBounds",
        PropertyMetadata::Create(false, PropertyChangedCallback(OnConstrainToParentBoundsChanged)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const DependencyProperty* MouseDragElementBehavior::XProperty;
const DependencyProperty* MouseDragElementBehavior::YProperty;
const DependencyProperty* MouseDragElementBehavior::ConstrainToParentBoundsProperty;
