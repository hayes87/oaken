#include "AnimationSystem.h"
#include "../Components/Components.h"
#include "../Core/Log.h"
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <cmath>

namespace Systems {
    AnimationSystem::AnimationSystem(flecs::world& world) {
        world.system<AnimatorComponent>("AnimationSystem")
            .each([](flecs::entity e, AnimatorComponent& animator) {
                static bool loggedOnce = false;
                static bool loggedModels = false;
                float dt = e.world().delta_time();
                
                if (!animator.skeleton) return;
                
                // Resize buffers if needed
                int num_soa_joints = animator.skeleton->skeleton.num_soa_joints();
                int num_joints = animator.skeleton->skeleton.num_joints();
                
                if (animator.locals.size() != num_soa_joints) {
                    animator.locals.resize(num_soa_joints);
                }
                if (animator.blendLocals.size() != num_soa_joints) {
                    animator.blendLocals.resize(num_soa_joints);
                }
                if (animator.models.size() != num_joints) {
                    animator.models.resize(num_joints);
                }
                
                ozz::span<const ozz::math::SoaTransform> ltm_input;
                
                // Check if we're using AnimGraph
                if (animator.animGraph) {
                    // Update the graph instance
                    animator.graphInstance.Update(dt);
                    
                    auto samples = animator.graphInstance.GetCurrentSamples();
                    
                    // Debug: log track vs skeleton mismatch
                    if (!loggedOnce && !samples.empty()) {
                        for (size_t i = 0; i < samples.size(); i++) {
                            LOG_INFO("AnimSystem: sample[{}] tracks={}, skeleton soa_joints={}, joints={}", 
                                i,
                                samples[i].animation->animation.num_tracks(),
                                num_soa_joints, num_joints);
                        }
                        loggedOnce = true;
                    }
                    
                    if (samples.empty()) {
                        // No animation, use rest pose
                        ltm_input = animator.skeleton->skeleton.joint_rest_poses();
                    } else if (samples.size() == 1) {
                        // Single animation (no blending)
                        auto& sample = samples[0];
                        
                        if (!animator.context) {
                            animator.context = std::make_unique<ozz::animation::SamplingJob::Context>();
                        }
                        if (animator.context->max_tracks() < sample.animation->animation.num_tracks()) {
                            animator.context->Resize(sample.animation->animation.num_tracks());
                        }
                        
                        float duration = sample.animation->animation.duration();
                        float ratio = (duration > 0.0f) ? (sample.time / duration) : 0.0f;
                        if (sample.loop) {
                            ratio = std::fmod(ratio, 1.0f);
                            if (ratio < 0.0f) ratio += 1.0f;
                        } else {
                            ratio = std::clamp(ratio, 0.0f, 1.0f);
                        }
                        
                        ozz::animation::SamplingJob sampling_job;
                        sampling_job.animation = &sample.animation->animation;
                        sampling_job.context = animator.context.get();
                        sampling_job.ratio = ratio;
                        sampling_job.output = make_span(animator.locals);
                        
                        if (!sampling_job.Run()) {
                            ltm_input = animator.skeleton->skeleton.joint_rest_poses();
                        } else {
                            ltm_input = make_span(animator.locals);
                        }
                    } else {
                        // Blending two animations
                        auto& sample1 = samples[0];  // Previous state
                        auto& sample2 = samples[1];  // Current state
                        
                        // Debug logging once
                        if (!loggedOnce) {
                            LOG_INFO("AnimBlend: skeleton soa_joints={}, joints={}", 
                                animator.skeleton->skeleton.num_soa_joints(),
                                animator.skeleton->skeleton.num_joints());
                            LOG_INFO("AnimBlend: anim1 tracks={}, anim2 tracks={}",
                                sample1.animation->animation.num_tracks(),
                                sample2.animation->animation.num_tracks());
                            loggedOnce = true;
                        }
                        
                        // Ensure contexts exist
                        if (!animator.context) {
                            animator.context = std::make_unique<ozz::animation::SamplingJob::Context>();
                        }
                        if (!animator.blendContext) {
                            animator.blendContext = std::make_unique<ozz::animation::SamplingJob::Context>();
                        }
                        
                        // Resize contexts
                        if (animator.context->max_tracks() < sample1.animation->animation.num_tracks()) {
                            animator.context->Resize(sample1.animation->animation.num_tracks());
                        }
                        if (animator.blendContext->max_tracks() < sample2.animation->animation.num_tracks()) {
                            animator.blendContext->Resize(sample2.animation->animation.num_tracks());
                        }
                        
                        // Sample first animation
                        float duration1 = sample1.animation->animation.duration();
                        float ratio1 = (duration1 > 0.0f) ? (sample1.time / duration1) : 0.0f;
                        if (sample1.loop) ratio1 = std::fmod(ratio1, 1.0f);
                        ratio1 = std::clamp(ratio1, 0.0f, 1.0f);
                        
                        ozz::animation::SamplingJob job1;
                        job1.animation = &sample1.animation->animation;
                        job1.context = animator.context.get();
                        job1.ratio = ratio1;
                        job1.output = make_span(animator.locals);
                        job1.Run();
                        
                        // Sample second animation
                        float duration2 = sample2.animation->animation.duration();
                        float ratio2 = (duration2 > 0.0f) ? (sample2.time / duration2) : 0.0f;
                        if (sample2.loop) ratio2 = std::fmod(ratio2, 1.0f);
                        ratio2 = std::clamp(ratio2, 0.0f, 1.0f);
                        
                        ozz::animation::SamplingJob job2;
                        job2.animation = &sample2.animation->animation;
                        job2.context = animator.blendContext.get();
                        job2.ratio = ratio2;
                        job2.output = make_span(animator.blendLocals);
                        job2.Run();
                        
                        // Blend the two
                        ozz::animation::BlendingJob::Layer layers[2];
                        layers[0].transform = make_span(animator.locals);
                        layers[0].weight = sample1.weight;
                        layers[1].transform = make_span(animator.blendLocals);
                        layers[1].weight = sample2.weight;
                        
                        ozz::animation::BlendingJob blend_job;
                        blend_job.layers = ozz::span<ozz::animation::BlendingJob::Layer>(layers, 2);
                        blend_job.rest_pose = animator.skeleton->skeleton.joint_rest_poses();
                        blend_job.output = make_span(animator.locals);
                        blend_job.threshold = 0.1f;
                        
                        if (!blend_job.Run()) {
                            ltm_input = animator.skeleton->skeleton.joint_rest_poses();
                        } else {
                            ltm_input = make_span(animator.locals);
                        }
                    }
                } else if (animator.animation) {
                    // Legacy single animation mode
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
                    LOG_ERROR("LocalToModelJob failed!");
                }
                
                // Debug: check if models look reasonable
                if (!loggedModels && animator.models.size() > 10) {
                    for (int i = 0; i < 5; i++) {
                        const auto& m = animator.models[i];
                        glm::vec3 pos(
                            ozz::math::GetX(m.cols[3]),
                            ozz::math::GetY(m.cols[3]),
                            ozz::math::GetZ(m.cols[3])
                        );
                        LOG_INFO("Model[{}] pos: ({}, {}, {})", i, pos.x, pos.y, pos.z);
                    }
                    loggedModels = true;
                }
            });
    }
}
