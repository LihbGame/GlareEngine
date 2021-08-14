#pragma once
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/options/options.h"
#include "ozz/framework/application.h"
#include "ozz/framework/imgui.h"
#include "ozz/framework/renderer.h"
#include "ozz/framework/utils.h"
#include "ozz/framework/mesh.h"
#include <map>
//using namespace ozz;

// Skeleton archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(skeleton,
    "Path to the skeleton (ozz archive format).",
    "model/OZZ/skeleton.ozz", false)

// Animation archive can be specified as an option.
OZZ_OPTIONS_DECLARE_STRING(animation,
     "Path to the animation (ozz archive format).",
     "model/OZZ/Take001.ozz", false)



class AnimePlayback
{
public:
    AnimePlayback() {}
    // Updates current animation time and skeleton pose.
    virtual bool OnUpdate(float _dt, float)
    {
        // Updates current animation time.
        controller_.Update(animation_, _dt);

        // Samples optimized animation at t = animation_time_.
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = &animation_;
        sampling_job.cache = &cache_;
        sampling_job.ratio = controller_.time_ratio();
        sampling_job.output = make_span(locals_);
        if (!sampling_job.Run())
        {
            return false;
        }

        // Converts from local space to model space matrices.
        ozz::animation::LocalToModelJob ltm_job;
        ltm_job.skeleton = &skeleton_;
        ltm_job.input = make_span(locals_);
        ltm_job.output = make_span(models_);
        if (!ltm_job.Run())
        {
            return false;
        }


        return true;
    }
    // Samples animation, transforms to model space and renders.
 

    virtual bool OnInitialize() {
        // Reading skeleton.
        if (!ozz::sample::LoadSkeleton(OPTIONS_skeleton, &skeleton_)) {
            return false;
        }

        // Reading animation.
        if (!ozz::sample::LoadAnimation(OPTIONS_animation, &animation_)) {
            return false;
        }
        // Reading bone mesh .
        if (!ozz::sample::LoadMesh("model/OZZ/mesh.ozz", &mesh_)) {
            return false;
        }
        // Skeleton and animation needs to match.
        if (skeleton_.num_joints() != animation_.num_tracks()) {
            return false;
        }
        name=skeleton_.joint_names();
        for (int i=0;i<name.size();i++)
        {
            Lname[string(name[i])] = i;
#if defined(_DEBUG)
            string DebugInfo(string(name[i]) +"\n");
            ::OutputDebugStringA(DebugInfo.c_str());
#endif
        }
        // Allocates runtime buffers.
        const int num_soa_joints = skeleton_.num_soa_joints();
        locals_.resize(num_soa_joints);
        const int num_joints = skeleton_.num_joints();
        models_.resize(num_joints);

        // Allocates a cache that matches animation requirements.
        cache_.Resize(num_joints);

        return true;
    }

    virtual void OnDestroy() {}

    virtual bool OnGui(ozz::sample::ImGui* _im_gui) {
        // Exposes animation runtime playback controls.
        {
            static bool open = true;
            ozz::sample::ImGui::OpenClose oc(_im_gui, "Animation control", &open);
            if (open) {
                controller_.OnGui(animation_, _im_gui);
            }
        }
        return true;
    }

    virtual void GetSceneBounds(ozz::math::Box* _bound) const {
        ozz::sample::ComputePostureBounds(make_span(models_), _bound);
    }

    XMFLOAT4X4 OZZToXM(ozz::math::Float4x4 matr)
    {
        XMFLOAT4X4 lmat;

        lmat._11 = matr.cols[0].m128_f32[0];
        lmat._12 = matr.cols[0].m128_f32[1];
        lmat._13 = matr.cols[0].m128_f32[2];
        lmat._14 = matr.cols[0].m128_f32[3];

        lmat._21 = matr.cols[1].m128_f32[0];
        lmat._22 = matr.cols[1].m128_f32[1];
        lmat._23 = matr.cols[1].m128_f32[2];
        lmat._24 = matr.cols[1].m128_f32[3];

        lmat._31 = matr.cols[2].m128_f32[0];
        lmat._32 = matr.cols[2].m128_f32[1];
        lmat._33 = matr.cols[2].m128_f32[2];
        lmat._34 = matr.cols[2].m128_f32[3];

        lmat._41 = matr.cols[3].m128_f32[0];
        lmat._42 = matr.cols[3].m128_f32[1];
        lmat._43 = matr.cols[3].m128_f32[2];
        lmat._44 = matr.cols[3].m128_f32[3];

        return lmat;
    }


    vector<XMFLOAT4X4> GetModelTransforms()
    {
        vector<XMFLOAT4X4> ModelTransforms;
        ModelTransforms.resize(models_.size());
        for (int i=0;i< models_.size();++i)
        {
            ModelTransforms[i]=OZZToXM(models_[i]);
            XMStoreFloat4x4(&ModelTransforms[i], XMMatrixTranspose(XMLoadFloat4x4(&ModelTransforms[i])));
        }
        return ModelTransforms;
    }



    std::map<string, int> GetBoneNames()
    {
        return  Lname;
    }



private:
    ozz::span<const char* const> name;
    map<string,int> Lname;
    ozz::sample::Mesh  mesh_;
    // Playback animation controller. This is a utility class that helps with
    // controlling animation playback time.
    ozz::sample::PlaybackController controller_;

    // Runtime skeleton.
    ozz::animation::Skeleton skeleton_;

    // Runtime animation.
    ozz::animation::Animation animation_;

    // Sampling cache.
    ozz::animation::SamplingCache cache_;

    // Buffer of local transforms as sampled from animation_.
    ozz::vector<ozz::math::SoaTransform> locals_;

    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> models_;
    ozz::vector<ozz::math::Float4x4> skinning_matrices_;
};


