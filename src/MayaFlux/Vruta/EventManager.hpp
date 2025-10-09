#pragma once

#include <utility>

#include "Event.hpp"

namespace MayaFlux::Vruta {

class EventManager {
public:
    EventManager() = default;

    /**
     * @brief Add a event to the manager
     * @param event Event to add
     * @param name Optional name for the event (for event management)
     */
    void add_event(const std::shared_ptr<Event>& event, const std::string& name = "");

    /**
     * @brief Remove an event by name
     */
    bool remove_event(const std::string& name);

    /**
     * @brief Get a named event
     * @param name event name
     * @return Shared pointer to event or nullptr
     */
    std::shared_ptr<Event> get_event(const std::string& name) const;

    /**
     * @brief Get all managed events
     */
    std::vector<std::shared_ptr<Event>> get_all_events() const;

    /**
     * @brief Cancels and removes a event from the manager
     * @param event Shared pointer to the event to cancel
     * @return True if the event was found and cancelled, false otherwise
     *
     * This method removes a event from the manager, preventing it from
     * executing further. It's used to stop events that are no longer needed
     * or to clean up before shutting down the engine.
     */
    bool cancel_event(const std::shared_ptr<Event>& event);

    /**
     * @brief Cancel a event by name
     * @param name event name to cancel
     * @return True if found and cancelled
     */
    bool cancel_event(const std::string& name);

    /**
     * @brief Update parameters of a named event
     * @tparam Args Parameter types
     * @param name event name
     * @param args New parameters
     * @return True if event found and updated
     */
    template <typename... Args>
    bool update_event_params(const std::string& name, Args&&... args)
    {
        if (auto event = find_event_by_name(name); event && event->is_active()) {
            event->update_params(std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Get event state value by name and key
     * @tparam T State value type
     * @param name event name
     * @param state_key State key
     * @return Pointer to value or nullptr
     */
    template <typename T>
    T* get_event_state(const std::string& name, const std::string& state_key) const
    {
        if (auto event = find_event_by_name(name); event && event->is_active()) {
            return event->get_state<T>(state_key);
        }
        return nullptr;
    }

    /**
     * @brief Create value accessor function for named event
     * @tparam T Value type
     * @param name event name
     * @param state_key State key
     * @return Function returning current value
     */
    template <typename T>
    std::function<T()> create_value_accessor(const std::string& name, const std::string& state_key) const
    {
        return [this, name, state_key]() -> T {
            if (auto value = get_event_state<T>(name, state_key)) {
                return *value;
            }
            return T {};
        };
    }

    /**
     * @brief Generates a unique event ID for new events
     * @return A unique event ID
     */
    uint64_t get_next_event_id() const;

    /**
     * @brief Check if the manager has any active events
     * @return True if the manager has active events
     */
    bool has_active_events() const;

    /**
     * @brief Get all event names for debugging/inspection
     * @return Vector of all event names
     */
    std::vector<std::string> get_event_names() const;

    /**
     * @brief Terminate and clear all events
     */
    void terminate_all_events();

private:
    /**
     * @brief Generate automatic name for a event based on its type
     * @param event The event to name
     * @return Generated name
     */
    std::string auto_generate_name(const std::shared_ptr<Event>& event) const;

    /**
     * @brief Find event entry by name
     * @param name event name to find
     * @return Event
     */
    std::shared_ptr<Event> find_event_by_name(const std::string& name);

    /**
     * @brief Find event entry by name (const version)
     * @param name event name to find
     * @return Const iterator to event entry or end()
     */
    std::shared_ptr<Event> find_event_by_name(const std::string& name) const;

    /**
     * @brief Clean up completed events
     */
    void cleanup_completed_events();

    /**
     * @brief event ID counter for unique identification
     */
    mutable std::atomic<uint64_t> m_next_event_id { 1 };

    std::vector<std::shared_ptr<Event>> m_events;
    std::unordered_map<std::string, std::shared_ptr<Event>> m_named_events;
};

}
