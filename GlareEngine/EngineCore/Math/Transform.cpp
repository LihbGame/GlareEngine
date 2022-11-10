#include "Transform.h"
#include "BoundingSphere.h"

BoundingSphere GlareEngine::Math::ScaleAndTranslation::operator*(const BoundingSphere& sphere) const
{
    Vector4 scaledSphere = (Vector4)sphere * GetScale();
    Vector4 translation = Vector4(SetWToZero(m_repr));
    return BoundingSphere(scaledSphere + translation);
}
