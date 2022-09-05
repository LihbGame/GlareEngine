#include "Animation.h"
#include "Model.h"
#include "Math/Common.h"
#include "Math/Quaternion.h"

using Math::Quaternion;
using Math::Vector4;

//int to 0-1 float
static inline float ToFloat(const int8_t x) { return Math::Max(x / 127.0f, -1.0f); }
static inline float ToFloat(const uint8_t x) { return x / 255.0f; }
static inline float ToFloat(const int16_t x) { return Math::Max(x / 32767.0f, -1.0f); }
static inline float ToFloat(const uint16_t x) { return x / 65535.0f; }

//lerp float3
static inline void Lerp3(float* Dest, const float* Key1, const float* Key2, float T)
{
	Dest[0] = Math::Lerp(Key1[0], Key2[0], T);
	Dest[1] = Math::Lerp(Key1[1], Key2[1], T);
	Dest[2] = Math::Lerp(Key1[2], Key2[2], T);
}

template <typename T>
static inline Quaternion ToQuat(const T* rot)
{
	return (Quaternion)Vector4(ToFloat(rot[0]), ToFloat(rot[1]), ToFloat(rot[2]), ToFloat(rot[3]));
}

static inline Quaternion ToQuat(const float* rot)
{
	return Quaternion(XMLoadFloat4((const XMFLOAT4*)rot));
}


static inline void Slerp(float* Dest, const void* Key1, const void* Key2, float T, uint32_t Format)
{
	switch (Format)
	{
	case AnimationCurve::eSNorm8:
	{
		const int8_t* key1 = (const int8_t*)Key1;
		const int8_t* key2 = (const int8_t*)Key2;
		XMStoreFloat4((XMFLOAT4*)Dest, (FXMVECTOR)Math::Slerp(ToQuat(key1), ToQuat(key2), T));
		break;
	}
	case AnimationCurve::eUNorm8:
	{
		const uint8_t* key1 = (const uint8_t*)Key1;
		const uint8_t* key2 = (const uint8_t*)Key2;
		XMStoreFloat4((XMFLOAT4*)Dest, (FXMVECTOR)Math::Slerp(ToQuat(key1), ToQuat(key2), T));
		break;
	}
	case AnimationCurve::eSNorm16:
	{
		const int16_t* key1 = (const int16_t*)Key1;
		const int16_t* key2 = (const int16_t*)Key2;
		XMStoreFloat4((XMFLOAT4*)Dest, (FXMVECTOR)Math::Slerp(ToQuat(key1), ToQuat(key2), T));
		break;
	}
	case AnimationCurve::eUNorm16:
	{
		const uint16_t* key1 = (const uint16_t*)Key1;
		const uint16_t* key2 = (const uint16_t*)Key2;
		XMStoreFloat4((XMFLOAT4*)Dest, (FXMVECTOR)Math::Slerp(ToQuat(key1), ToQuat(key2), T));
		break;
	}
	case AnimationCurve::eFloat:
	{
		const float* key1 = (const float*)Key1;
		const float* key2 = (const float*)Key2;
		XMStoreFloat4((XMFLOAT4*)Dest, (FXMVECTOR)Math::Slerp(ToQuat(key1), ToQuat(key2), T));
		break;
	}
	default:
		//Unexpected animation key frame data format
		assert(0);
		break;
	}
}


void ModelInstance::UpdateAnimations(float deltaTime)
{
	uint32_t NumAnimations = m_Model->m_NumAnimations;
	GraphNode* animGraph = m_AnimGraph.get();

	for (uint32_t i = 0; i < NumAnimations; ++i)
	{
		AnimationState& anim = m_AnimState[i];
		if (anim.state == AnimationState::eStopped)
			continue;

		anim.time += deltaTime;

		const AnimationSet& animation = m_Model->m_Animations[i];

		if (anim.state == AnimationState::eLooping)
		{
			anim.time = fmodf(anim.time, animation.duration);
		}
		else if (anim.time > animation.duration)
		{
			anim.time = 0.0f;
			anim.state = AnimationState::eStopped;
		}

		const AnimationCurve* firstCurve = m_Model->m_CurveData.get() + animation.firstCurve;

		// Update animation nodes
		for (uint32_t j = 0; j < animation.numCurves; ++j)
		{
			const AnimationCurve& curve = firstCurve[j];
			assert(curve.NumSegments > 0);

			const float progress = Math::Clamp((anim.time - curve.StartTime) * curve.RangeScale, 0.0f, curve.NumSegments);
			const uint32_t segment = (uint32_t)progress;
			const float lerpT = progress - (float)segment;

			const size_t stride = curve.KeyFrameStride * 4;
			const byte* key1 = m_Model->m_KeyFrameData.get() + curve.KeyFrameOffset + stride * segment;
			const byte* key2 = key1 + stride;
			GraphNode& node = animGraph[curve.TargetNode];

			switch (curve.TargetTransform)
			{
			case AnimationCurve::eTranslation:
				assert(curve.KeyFrameFormat == AnimationCurve::eFloat);
				Lerp3((float*)&node.transform + 12, (const float*)key1, (const float*)key2, lerpT);
				break;
			case AnimationCurve::eRotation:
				node.staleMatrix = true;
				Slerp((float*)&node.rotation, key1, key2, lerpT, curve.KeyFrameFormat);
				break;
			case AnimationCurve::eScale:
				assert(curve.KeyFrameFormat == AnimationCurve::eFloat);
				node.staleMatrix = true;
				Lerp3((float*)&node.scale, (const float*)key1, (const float*)key2, lerpT);
				break;
			default:
			case AnimationCurve::eWeights:
				//Unhandled blend shape weights in animation
				assert(0);
				break;
			}
		}
	}
}

void ModelInstance::PlayAnimation(uint32_t animIdx, bool loop)
{
	if (animIdx < m_AnimState.size())
		m_AnimState[animIdx].state = loop ? AnimationState::eLooping : AnimationState::ePlaying;
}

void ModelInstance::PauseAnimation(uint32_t animIdx)
{
	if (animIdx < m_AnimState.size())
		m_AnimState[animIdx].state = AnimationState::eStopped;
}

void ModelInstance::ResetAnimation(uint32_t animIdx)
{
	if (animIdx < m_AnimState.size())
		m_AnimState[animIdx].time = 0.0f;
}

void ModelInstance::StopAnimation(uint32_t animIdx)
{
	if (animIdx < m_AnimState.size())
	{
		m_AnimState[animIdx].state = AnimationState::eStopped;
		m_AnimState[animIdx].time = 0.0f;
	}
}

void ModelInstance::LoopAllAnimations(void)
{
	for (auto& anim : m_AnimState)
	{
		anim.state = AnimationState::eLooping;
		anim.time = 0.0f;
	}
}

