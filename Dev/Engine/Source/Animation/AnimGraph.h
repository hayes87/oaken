#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "../Resources/Animation.h"

namespace Animation {

    // Parameter types for controlling transitions
    struct AnimParameter {
        enum class Type { Float, Bool, Trigger };
        Type type = Type::Float;
        float floatValue = 0.0f;
        bool boolValue = false;
        bool triggerValue = false; // Auto-resets after consumption
    };

    // Condition for a transition
    struct TransitionCondition {
        std::string paramName;
        enum class Comparison { Equals, NotEquals, Greater, Less, GreaterEquals, LessEquals };
        Comparison comparison = Comparison::Equals;
        float threshold = 0.0f; // For float comparisons
        bool boolValue = true;  // For bool comparisons
        
        bool Evaluate(const AnimParameter& param) const;
    };

    // Transition between states
    struct AnimTransition {
        std::string targetState;
        float duration = 0.2f;  // Crossfade duration in seconds
        bool hasExitTime = false;
        float exitTime = 1.0f;  // Normalized time (0-1) when transition can occur
        std::vector<TransitionCondition> conditions;
        
        bool CanTransition(const std::unordered_map<std::string, AnimParameter>& params, 
                          float normalizedTime) const;
    };

    // Animation state (node in the graph)
    struct AnimState {
        std::string name;
        std::shared_ptr<Resources::Animation> animation;
        float speed = 1.0f;
        bool loop = true;
        std::vector<AnimTransition> transitions;
    };

    // The animation graph - defines states and transitions
    class AnimGraph {
    public:
        AnimGraph() = default;
        
        // Build the graph
        void AddState(const std::string& name, std::shared_ptr<Resources::Animation> anim, 
                     float speed = 1.0f, bool loop = true);
        void SetDefaultState(const std::string& name);
        void AddTransition(const std::string& fromState, const std::string& toState,
                          float duration = 0.2f, bool hasExitTime = false, float exitTime = 1.0f);
        void AddTransitionCondition(const std::string& fromState, const std::string& toState,
                                   const std::string& paramName, 
                                   TransitionCondition::Comparison comp, float threshold);
        void AddTransitionConditionBool(const std::string& fromState, const std::string& toState,
                                        const std::string& paramName, bool value);
        
        // Parameter management
        void AddParameter(const std::string& name, AnimParameter::Type type, float defaultValue = 0.0f);
        void AddParameterBool(const std::string& name, bool defaultValue = false);
        void AddTrigger(const std::string& name);
        
        // Accessors
        const AnimState* GetState(const std::string& name) const;
        const AnimState* GetDefaultState() const;
        const std::unordered_map<std::string, AnimState>& GetStates() const { return m_States; }
        std::unordered_map<std::string, AnimParameter> CreateParameterInstance() const;
        
    private:
        std::unordered_map<std::string, AnimState> m_States;
        std::unordered_map<std::string, AnimParameter> m_DefaultParams;
        std::string m_DefaultState;
    };

    // Runtime state for an animated entity
    struct AnimGraphInstance {
        const AnimGraph* graph = nullptr;
        std::string currentState;
        std::string previousState;     // For blending during transition
        float stateTime = 0.0f;        // Time in current state
        float previousStateTime = 0.0f; // Time in previous state (for blending)
        float transitionTime = 0.0f;   // Time into transition
        float transitionDuration = 0.0f;
        bool isTransitioning = false;
        std::unordered_map<std::string, AnimParameter> parameters;
        
        // Initialize from graph
        void Init(const AnimGraph* g);
        
        // Parameter setters
        void SetFloat(const std::string& name, float value);
        void SetBool(const std::string& name, bool value);
        void SetTrigger(const std::string& name);
        
        // Parameter getters
        float GetFloat(const std::string& name) const;
        bool GetBool(const std::string& name) const;
        
        // Update and get blend weight (0 = previous state, 1 = current state)
        void Update(float dt);
        float GetBlendWeight() const;
        
        // Get current animations and their weights for sampling
        struct AnimationSample {
            std::shared_ptr<Resources::Animation> animation;
            float time;
            float weight;
            bool loop;
        };
        std::vector<AnimationSample> GetCurrentSamples() const;
    };

} // namespace Animation
