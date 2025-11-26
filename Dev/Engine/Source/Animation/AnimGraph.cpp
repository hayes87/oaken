#include "AnimGraph.h"
#include <algorithm>

namespace Animation {

    // TransitionCondition
    bool TransitionCondition::Evaluate(const AnimParameter& param) const {
        if (param.type == AnimParameter::Type::Trigger) {
            return param.triggerValue;
        }
        
        if (param.type == AnimParameter::Type::Bool) {
            switch (comparison) {
                case Comparison::Equals: return param.boolValue == boolValue;
                case Comparison::NotEquals: return param.boolValue != boolValue;
                default: return param.boolValue == boolValue;
            }
        }
        
        // Float comparison
        switch (comparison) {
            case Comparison::Equals: return param.floatValue == threshold;
            case Comparison::NotEquals: return param.floatValue != threshold;
            case Comparison::Greater: return param.floatValue > threshold;
            case Comparison::Less: return param.floatValue < threshold;
            case Comparison::GreaterEquals: return param.floatValue >= threshold;
            case Comparison::LessEquals: return param.floatValue <= threshold;
        }
        return false;
    }

    // AnimTransition
    bool AnimTransition::CanTransition(const std::unordered_map<std::string, AnimParameter>& params,
                                       float normalizedTime) const {
        // Check exit time
        if (hasExitTime && normalizedTime < exitTime) {
            return false;
        }
        
        // Check all conditions (AND logic)
        for (const auto& cond : conditions) {
            auto it = params.find(cond.paramName);
            if (it == params.end()) return false;
            if (!cond.Evaluate(it->second)) return false;
        }
        
        return true;
    }

    // AnimGraph
    void AnimGraph::AddState(const std::string& name, std::shared_ptr<Resources::Animation> anim,
                            float speed, bool loop) {
        AnimState state;
        state.name = name;
        state.animation = anim;
        state.speed = speed;
        state.loop = loop;
        m_States[name] = std::move(state);
        
        if (m_DefaultState.empty()) {
            m_DefaultState = name;
        }
    }

    void AnimGraph::SetDefaultState(const std::string& name) {
        m_DefaultState = name;
    }

    void AnimGraph::AddTransition(const std::string& fromState, const std::string& toState,
                                  float duration, bool hasExitTime, float exitTime) {
        auto it = m_States.find(fromState);
        if (it == m_States.end()) return;
        
        AnimTransition trans;
        trans.targetState = toState;
        trans.duration = duration;
        trans.hasExitTime = hasExitTime;
        trans.exitTime = exitTime;
        it->second.transitions.push_back(trans);
    }

    void AnimGraph::AddTransitionCondition(const std::string& fromState, const std::string& toState,
                                           const std::string& paramName,
                                           TransitionCondition::Comparison comp, float threshold) {
        auto it = m_States.find(fromState);
        if (it == m_States.end()) return;
        
        for (auto& trans : it->second.transitions) {
            if (trans.targetState == toState) {
                TransitionCondition cond;
                cond.paramName = paramName;
                cond.comparison = comp;
                cond.threshold = threshold;
                trans.conditions.push_back(cond);
                return;
            }
        }
    }

    void AnimGraph::AddTransitionConditionBool(const std::string& fromState, const std::string& toState,
                                               const std::string& paramName, bool value) {
        auto it = m_States.find(fromState);
        if (it == m_States.end()) return;
        
        for (auto& trans : it->second.transitions) {
            if (trans.targetState == toState) {
                TransitionCondition cond;
                cond.paramName = paramName;
                cond.comparison = TransitionCondition::Comparison::Equals;
                cond.boolValue = value;
                trans.conditions.push_back(cond);
                return;
            }
        }
    }

    void AnimGraph::AddParameter(const std::string& name, AnimParameter::Type type, float defaultValue) {
        AnimParameter param;
        param.type = type;
        param.floatValue = defaultValue;
        m_DefaultParams[name] = param;
    }

    void AnimGraph::AddParameterBool(const std::string& name, bool defaultValue) {
        AnimParameter param;
        param.type = AnimParameter::Type::Bool;
        param.boolValue = defaultValue;
        m_DefaultParams[name] = param;
    }

    void AnimGraph::AddTrigger(const std::string& name) {
        AnimParameter param;
        param.type = AnimParameter::Type::Trigger;
        param.triggerValue = false;
        m_DefaultParams[name] = param;
    }

    const AnimState* AnimGraph::GetState(const std::string& name) const {
        auto it = m_States.find(name);
        return it != m_States.end() ? &it->second : nullptr;
    }

    const AnimState* AnimGraph::GetDefaultState() const {
        return GetState(m_DefaultState);
    }

    std::unordered_map<std::string, AnimParameter> AnimGraph::CreateParameterInstance() const {
        return m_DefaultParams;
    }

    // AnimGraphInstance
    void AnimGraphInstance::Init(const AnimGraph* g) {
        graph = g;
        parameters = g->CreateParameterInstance();
        
        const AnimState* defaultState = g->GetDefaultState();
        if (defaultState) {
            currentState = defaultState->name;
        }
        
        stateTime = 0.0f;
        transitionTime = 0.0f;
        transitionDuration = 0.0f;
        isTransitioning = false;
        previousState.clear();
    }

    void AnimGraphInstance::SetFloat(const std::string& name, float value) {
        auto it = parameters.find(name);
        if (it != parameters.end() && it->second.type == AnimParameter::Type::Float) {
            it->second.floatValue = value;
        }
    }

    void AnimGraphInstance::SetBool(const std::string& name, bool value) {
        auto it = parameters.find(name);
        if (it != parameters.end() && it->second.type == AnimParameter::Type::Bool) {
            it->second.boolValue = value;
        }
    }

    void AnimGraphInstance::SetTrigger(const std::string& name) {
        auto it = parameters.find(name);
        if (it != parameters.end() && it->second.type == AnimParameter::Type::Trigger) {
            it->second.triggerValue = true;
        }
    }

    float AnimGraphInstance::GetFloat(const std::string& name) const {
        auto it = parameters.find(name);
        return (it != parameters.end()) ? it->second.floatValue : 0.0f;
    }

    bool AnimGraphInstance::GetBool(const std::string& name) const {
        auto it = parameters.find(name);
        return (it != parameters.end()) ? it->second.boolValue : false;
    }

    void AnimGraphInstance::Update(float dt) {
        if (!graph) return;
        
        const AnimState* state = graph->GetState(currentState);
        if (!state || !state->animation) return;
        
        float duration = state->animation->animation.duration();
        float normalizedTime = (duration > 0.0f) ? (stateTime / duration) : 0.0f;
        
        // Update transition
        if (isTransitioning) {
            transitionTime += dt;
            if (transitionTime >= transitionDuration) {
                // Transition complete
                isTransitioning = false;
                previousState.clear();
                transitionTime = 0.0f;
            }
        }
        
        // Check for new transitions (only if not already transitioning)
        if (!isTransitioning) {
            for (const auto& trans : state->transitions) {
                if (trans.CanTransition(parameters, normalizedTime)) {
                    // Start transition
                    previousState = currentState;
                    previousStateTime = stateTime;  // Save current time for blending
                    currentState = trans.targetState;
                    transitionDuration = trans.duration;
                    transitionTime = 0.0f;
                    isTransitioning = (transitionDuration > 0.0f);
                    stateTime = 0.0f;  // Reset time for new state
                    
                    // Consume triggers
                    for (auto& [name, param] : parameters) {
                        if (param.type == AnimParameter::Type::Trigger) {
                            param.triggerValue = false;
                        }
                    }
                    break;
                }
            }
        }
        
        // Update state time
        stateTime += dt * state->speed;
        
        // Update previous state time if transitioning
        if (isTransitioning && !previousState.empty()) {
            const AnimState* prevState = graph->GetState(previousState);
            if (prevState) {
                previousStateTime += dt * prevState->speed;
                // Handle looping for previous state too
                float prevDuration = prevState->animation ? prevState->animation->animation.duration() : 1.0f;
                if (prevState->loop && prevDuration > 0.0f) {
                    while (previousStateTime > prevDuration) {
                        previousStateTime -= prevDuration;
                    }
                }
            }
        }
        
        // Handle looping
        if (state->loop && duration > 0.0f) {
            while (stateTime > duration) {
                stateTime -= duration;
            }
        } else if (stateTime > duration) {
            stateTime = duration;
        }
    }

    float AnimGraphInstance::GetBlendWeight() const {
        if (!isTransitioning || transitionDuration <= 0.0f) {
            return 1.0f;
        }
        return std::clamp(transitionTime / transitionDuration, 0.0f, 1.0f);
    }

    std::vector<AnimGraphInstance::AnimationSample> AnimGraphInstance::GetCurrentSamples() const {
        std::vector<AnimationSample> samples;
        if (!graph) return samples;
        
        float blendWeight = GetBlendWeight();
        
        // Previous state (if transitioning)
        if (isTransitioning && !previousState.empty()) {
            const AnimState* prevState = graph->GetState(previousState);
            if (prevState && prevState->animation) {
                AnimationSample sample;
                sample.animation = prevState->animation;
                sample.time = previousStateTime;  // Use tracked previous time
                sample.weight = 1.0f - blendWeight;
                sample.loop = prevState->loop;
                samples.push_back(sample);
            }
        }
        
        // Current state
        const AnimState* currState = graph->GetState(currentState);
        if (currState && currState->animation) {
            AnimationSample sample;
            sample.animation = currState->animation;
            sample.time = stateTime;
            sample.weight = isTransitioning ? blendWeight : 1.0f;
            sample.loop = currState->loop;
            samples.push_back(sample);
        }
        
        return samples;
    }

} // namespace Animation
