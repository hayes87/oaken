#include "AnimationSystem.h"
#include "../Components/Components.h"
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <cmath>
#include <iostream>

namespace Systems {
    AnimationSystem::AnimationSystem(flecs::world& world) {
        world.system<AnimatorComponent>("AnimationSystem")
            .each([](flecs::entity e, AnimatorComponent& animator) {
                float dt = e.world().delta_time();
                
                if (!animator.skeleton) return;
                
                // Resize buffers if needed
                int num_soa_joints = animator.skeleton->skeleton.num_soa_joints();
                int num_joints = animator.skeleton->skeleton.num_joints();
                
                if (animator.locals.size() != num_soa_joints) {
                    animator.locals.resize(num_soa_joints);
                }
                if (animator.models.size() != num_joints) {
                    animator.models.resize(num_joints);
                }
                
                ozz::span<const ozz::math::SoaTransform> ltm_input;

                if (animator.animation) {
                    // Resize context if needed
                    if (!animator.context) {
                        animator.context = std::make_unique<ozz::animation::SamplingJob::Context>();
                    }
                    if (animator.context->max_tracks() < animator.animation->animation.num_tracks()) {
                        animator.context->Resize(animator.animation->animation.num_tracks());
                    }
                    
                    // Update time
                    animator.time += dt;
                    float duration = animator.animation->animation.duration();

                    if (animator.time > duration) {
                        if (animator.loop) {
                            animator.time = std::fmod(animator.time, duration);
                        } else {
                            animator.time = duration;
                        }
                    }
                    
                    // Sampling job
                    ozz::animation::SamplingJob sampling_job;
                    sampling_job.animation = &animator.animation->animation;
                    sampling_job.context = animator.context.get();
                    sampling_job.ratio = animator.time / duration;
                    sampling_job.output = make_span(animator.locals);
                    
                    if (!sampling_job.Run()) {
                        std::cerr << "[AnimationSystem] Sampling job failed!" << std::endl;
                        return;
                    }
                    ltm_input = make_span(animator.locals);
                } else {
                    // No animation, use rest pose
                    ltm_input = animator.skeleton->skeleton.joint_rest_poses();
                }
                
                // Local to Model job
                ozz::animation::LocalToModelJob ltm_job;
                ltm_job.skeleton = &animator.skeleton->skeleton;
                ltm_job.input = ltm_input;
                ltm_job.output = make_span(animator.models);
                
                if (!ltm_job.Run()) {
                    std::cerr << "[AnimationSystem] LTM job failed!" << std::endl;
                    return;
                }

                // Debug: Print first joint matrix occasionally
                static int logCounter = 0;
                if (logCounter++ % 60 == 0 && !animator.models.empty()) {
                    const auto& m = animator.models[0];
                    // ozz::math::Float4x4 is column-major
                    float x = ozz::math::GetX(m.cols[3]);
                    float y = ozz::math::GetY(m.cols[3]);
                    float z = ozz::math::GetZ(m.cols[3]);
                    
                    // Extract Scale (roughly)
                    float sx = ozz::math::GetX(ozz::math::Length3(m.cols[0]));
                    float sy = ozz::math::GetX(ozz::math::Length3(m.cols[1]));
                    float sz = ozz::math::GetX(ozz::math::Length3(m.cols[2]));

                    std::cout << "[AnimationSystem] Root Pos: " << x << ", " << y << ", " << z 
                              << " | Scale: " << sx << ", " << sy << ", " << sz << std::endl;
                }
            });
    }
}
