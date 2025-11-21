#include "AnimationSystem.h"
#include "../Components/Components.h"
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <cmath>

namespace Systems {
    AnimationSystem::AnimationSystem(flecs::world& world) {
        world.system<AnimatorComponent>("AnimationSystem")
            .each([](flecs::entity e, AnimatorComponent& animator) {
                float dt = e.world().delta_time();
                
                if (!animator.skeleton || !animator.animation) return;
                
                // Resize buffers if needed
                int num_soa_joints = animator.skeleton->skeleton.num_soa_joints();
                int num_joints = animator.skeleton->skeleton.num_joints();
                
                if (animator.locals.size() != num_soa_joints) {
                    animator.locals.resize(num_soa_joints);
                }
                if (animator.models.size() != num_joints) {
                    animator.models.resize(num_joints);
                }
                
                // Resize context if needed
                if (!animator.context) {
                    animator.context = std::make_unique<ozz::animation::SamplingJob::Context>();
                }
                if (animator.context->max_tracks() < animator.animation->animation.num_tracks()) {
                    animator.context->Resize(animator.animation->animation.num_tracks());
                }
                
                // Update time
                animator.time += dt;
                if (animator.time > animator.animation->animation.duration()) {
                    animator.time = std::fmod(animator.time, animator.animation->animation.duration());
                }
                
                // Sampling job
                ozz::animation::SamplingJob sampling_job;
                sampling_job.animation = &animator.animation->animation;
                sampling_job.context = animator.context.get();
                sampling_job.ratio = animator.time / animator.animation->animation.duration();
                sampling_job.output = make_span(animator.locals);
                
                if (!sampling_job.Run()) {
                    return;
                }
                
                // Local to Model job
                ozz::animation::LocalToModelJob ltm_job;
                ltm_job.skeleton = &animator.skeleton->skeleton;
                ltm_job.input = make_span(animator.locals);
                ltm_job.output = make_span(animator.models);
                
                if (!ltm_job.Run()) {
                    return;
                }
            });
    }
}
