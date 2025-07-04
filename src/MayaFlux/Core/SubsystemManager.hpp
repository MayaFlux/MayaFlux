#pragma once

#include "Subsystems/Subsystem.hpp"

namespace MayaFlux::Core {

/**
 * @class SubsystemManager
 * @brief Central coordinator for all subsystems in the MayaFlux processing architecture
 *
 * Manages subsystem lifecycle, provides token-scoped processing handles, and coordinates
 * cross-subsystem operations. Each subsystem receives a dedicated processing handle that
 * ensures proper isolation and thread safety within its designated processing domain.
 *
 * Key responsibilities:
 * - Subsystem registration and lifecycle management
 * - Processing handle creation and distribution
 * - Cross-subsystem data access control
 * - Coordinated startup and shutdown sequences
 */
class SubsystemManager {
public:
    /**
     * @brief Constructs SubsystemManager with required processing managers
     * @param node_graph_manager Shared node graph manager for all subsystems
     * @param buffer_manager Shared buffer manager for all subsystems
     *
     * Initializes the manager with references to the core processing systems.
     * These managers are shared across all subsystems but accessed through
     * token-scoped handles for proper isolation.
     */
    SubsystemManager(
        std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
        std::shared_ptr<Buffers::BufferManager> buffer_manager);

    /**
     * @brief Create and register a new subsystem with specified tokens
     * @tparam SubsystemType Type of subsystem to create
     * @tparam Args Constructor argument types
     * @param tokens Processing tokens that define the subsystem's domain
     * @param args Constructor arguments for the subsystem
     *
     * Template method for type-safe subsystem creation. The subsystem is
     * automatically registered, initialized with a token-scoped handle,
     * and prepared for operation.
     */
    template <typename SubsystemType, typename... Args>
    void create_subsystem(SubsystemTokens tokens, Args&&... args)
    {
        auto subsystem = std::make_unique<SubsystemType>(tokens, std::forward<Args>(args)...);
        add_subsystem(tokens, std::move(subsystem));
    }

    /** @brief Start all registered subsystems in coordination */
    void start_all_subsystems();

    /**
     * @brief Get typed access to a specific subsystem
     * @tparam SubsystemType Expected type of the subsystem
     * @param tokens Token configuration identifying the subsystem
     * @return Pointer to subsystem or nullptr if not found/wrong type
     *
     * Provides type-safe access to registered subsystems for direct
     * interaction when needed. Returns nullptr if subsystem doesn't
     * exist or type doesn't match.
     */
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
     *
     * The subsystem receives a handle that can only access its designated tokens.
     * The handle is owned by SubsystemManager but can be borrowed by the subsystem.
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

    /**
     * @brief Execute an operation with temporary elevated permissions
     * @tparam Func Function type for the operation
     * @param primary_tokens Primary subsystem tokens
     * @param secondary_tokens Secondary subsystem tokens for cross-domain access
     * @param operation Function to execute with combined access
     *
     * For special cross-domain operations (e.g., audio-reactive visuals).
     * Creates a temporary handle with combined token access for controlled
     * cross-subsystem operations.
     */
    template <typename Func>
    void execute_with_combined_tokens(
        SubsystemTokens primary_tokens,
        SubsystemTokens secondary_tokens,
        Func operation)
    {
        std::shared_lock lock(m_mutex);

        SubsystemTokens combined_tokens {
            .Buffer = primary_tokens.Buffer,
            .Node = primary_tokens.Node
        };

        SubsystemProcessingHandle temp_handle(
            m_buffer_manager,
            m_node_graph_manager,
            combined_tokens);

        operation(temp_handle);
    }

    /** @brief Shutdown all subsystems in proper order */
    void shutdown();

    /** @brief Configure cross-subsystem access permissions */
    void allow_cross_access(SubsystemTokens from, SubsystemTokens to);

    /**
     * @brief Get read-only access to another subsystem's data
     * @param requesting_tokens Tokens of the requesting subsystem
     * @param target_tokens Tokens of the target subsystem
     * @param channel Channel to read from
     * @return Read-only span if access allowed, nullopt otherwise
     *
     * Allows controlled cross-subsystem data sharing with permission checking.
     * Used for scenarios like audio-reactive visuals where one subsystem
     * needs to read another's processed data.
     */
    std::optional<std::span<const double>> read_cross_subsystem_buffer(
        SubsystemTokens requesting_tokens,
        SubsystemTokens target_tokens,
        u_int32_t channel);

private:
    bool is_cross_access_allowed(SubsystemTokens from, SubsystemTokens to) const;

    std::unordered_map<SubsystemTokens, std::unique_ptr<ISubsystem>> m_subsystems;
    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> m_buffer_manager;

    std::unordered_map<SubsystemTokens, std::unique_ptr<SubsystemProcessingHandle>> m_handles;
    std::unordered_map<SubsystemTokens, std::unordered_set<SubsystemTokens>> m_cross_access_permissions;

    mutable std::shared_mutex m_mutex; ///< Thread safety for subsystem operations
};

}
