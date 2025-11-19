#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <algorithm>

namespace Core {

    class EventBus {
    public:
        template<typename EventType>
        using Callback = std::function<void(const EventType&)>;

        template<typename EventType>
        void Subscribe(Callback<EventType> callback) {
            auto typeIdx = std::type_index(typeid(EventType));
            if (m_Subscribers.find(typeIdx) == m_Subscribers.end()) {
                m_Subscribers[typeIdx] = std::make_unique<EventDispatcher<EventType>>();
            }
            static_cast<EventDispatcher<EventType>*>(m_Subscribers[typeIdx].get())->Add(callback);
        }

        template<typename EventType>
        void Publish(const EventType& event) {
            auto typeIdx = std::type_index(typeid(EventType));
            if (m_Subscribers.find(typeIdx) != m_Subscribers.end()) {
                static_cast<EventDispatcher<EventType>*>(m_Subscribers[typeIdx].get())->Dispatch(event);
            }
        }

    private:
        struct IEventDispatcher {
            virtual ~IEventDispatcher() = default;
        };

        template<typename EventType>
        struct EventDispatcher : IEventDispatcher {
            std::vector<Callback<EventType>> callbacks;

            void Add(Callback<EventType> callback) {
                callbacks.push_back(callback);
            }

            void Dispatch(const EventType& event) {
                for (auto& callback : callbacks) {
                    callback(event);
                }
            }
        };

        std::unordered_map<std::type_index, std::unique_ptr<IEventDispatcher>> m_Subscribers;
    };

} // namespace Core
