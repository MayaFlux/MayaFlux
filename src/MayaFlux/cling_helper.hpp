// clinger_helpers.hpp - Helper functions for Cling that manage object lifetimes
#pragma once
#include "MayaFlux.hpp"
#include "MayaFlux/Kriya/Tasks.hpp"
#include "MayaFlux/Kriya/Timers.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/Node.hpp"
#include "MayaFlux/Vruta/Routine.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"
#include <memory>
#include <vector>

namespace MayaFlux::Cling {

// Global storage to prevent Cling from garbage collecting our objects
inline std::vector<std::shared_ptr<Nodes::Node>> persistent_nodes;
inline std::vector<std::shared_ptr<Vruta::SoundRoutine>> persistent_routines;
inline std::vector<std::any> persistent_objects;

// Helper to create and store nodes
template <typename NodeType, typename... Args>
std::shared_ptr<NodeType> make_node(Args&&... args)
{
    auto node = std::make_shared<NodeType>(std::forward<Args>(args)...);
    persistent_nodes.push_back(node);
    return node;
}

// Helper for chaining that stores the intermediate ChainNode
std::shared_ptr<Nodes::Node> chain(std::shared_ptr<Nodes::Node> a, std::shared_ptr<Nodes::Node> b)
{
    auto result = a >> b;
    persistent_nodes.push_back(result);
    return result;
}

// Helper for metro that keeps the routine alive
void metro(double interval, std::function<void()> callback)
{
    auto routine = std::make_shared<Vruta::SoundRoutine>(
        Kriya::metro(*get_scheduler(), interval, callback));
    persistent_routines.push_back(routine);
    get_scheduler()->add_task(routine, true);
}

// Helper for time operations
void play_for(std::shared_ptr<Nodes::Node> node, double seconds)
{
    // Create a persistent timer instead of using temporary objects
    Kriya::NodeTimer timer(*get_scheduler());
    timer.play_for(node, seconds);
    // Store the timer to prevent deallocation
    persistent_objects.push_back(timer);
}

// Clear all persistent storage (useful for cleanup)
void clear_all()
{
    persistent_nodes.clear();
    persistent_routines.clear();
    persistent_objects.clear();
}

// Aliases for common node types
inline auto sine(float freq, float amp)
{
    return make_node<Nodes::Generator::Sine>(freq, amp);
}
}
