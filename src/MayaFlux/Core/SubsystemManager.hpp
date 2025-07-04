#pragma once

#include "Subsystems/Subsystem.hpp"

namespace MayaFlux::Core {

class SubsystemManager {
private:
    std::unordered_map<SubsystemTokens, std::unique_ptr<ISubsystem>> m_subsystems;
    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> m_buffer_manager;

public:
    SubsystemManager(
        std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
        std::shared_ptr<Buffers::BufferManager> buffer_manager);

    void Initialize();

    template <typename SubsystemType, typename... Args>
    void create_subsystem(SubsystemTokens tokens, Args&&... args)
    {
        auto subsystem = std::make_unique<SubsystemType>(tokens, std::forward<Args>(args)...);
        subsystem->initialize(m_node_graph_manager, m_buffer_manager);
        subsystem->register_callbacks();

        m_subsystems[tokens] = std::move(subsystem);
    }

    void start_all_subsystems();

    template <typename SubsystemType>
    SubsystemType* get_subsystem(SubsystemTokens tokens)
    {
        if (auto it = m_subsystems.find(tokens); it != m_subsystems.end()) {
            return dynamic_cast<SubsystemType*>(it->second.get());
        }
        return nullptr;
    }

    /**
     * @brief Dynamically add a new subsystem to the manager
     * @param tokens Identifier for the subsystem
     * @param subsystem The subsystem instance
     */
    void add_subsystem(SubsystemTokens tokens, std::unique_ptr<ISubsystem> subsystem);

    /**
     * @brief Remove a subsystem from the manager
     * @param tokens Identifier for the subsystem
     */
    void remove_subsystem(SubsystemTokens tokens);

    /**
     * @brief Query the current status of all subsystems
     * @return Map of subsystem tokens to their ready/running status
     */
    std::unordered_map<SubsystemTokens, bool> query_subsystem_status() const;

    void shutdown();
};

}
