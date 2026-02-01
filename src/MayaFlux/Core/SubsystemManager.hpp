#pragma once

#include "Subsystems/Subsystem.hpp"

#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Core {

class WindowManager;
class AudioSubsystem;
class InputSubsystem;
class GraphicsSubsystem;

struct GlobalStreamInfo;
struct GlobalGraphicsConfig;
struct GlobalInputConfig;

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
class MAYAFLUX_API SubsystemManager {
public:
    /**
     * @brief Constructs SubsystemManager with required processing managers
     * @param node_graph_manager Shared node graph manager for all subsystems
     * @param buffer_manager Shared buffer manager for all subsystems
     * @param task_scheduler Shared task scheduler for all subsystems
     * @param window_manager Optional shared window manager for graphics subsystems
     * @param input_manager Optional shared input manager for input subsystems
     *
     * Initializes the manager with references to the core processing systems.
     * These managers are shared across all subsystems but accessed through
     * token-scoped handles for proper isolation.
     */
    SubsystemManager(
        std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
        std::shared_ptr<Buffers::BufferManager> buffer_manager,
        std::shared_ptr<Vruta::TaskScheduler> task_scheduler,
        std::shared_ptr<Core::WindowManager> window_manager = nullptr,
        std::shared_ptr<InputManager> input_manager = nullptr);

    /**
     * @brief Internal template method for type-safe subsystem creation
     * @tparam SType Type of subsystem to create
     * @tparam Args Constructor argument types
     * @param type SubsystemType enum value identifying the subsystem category
     * @param args Constructor arguments for the subsystem
     *
     * Creates a subsystem instance and registers it with the manager.
     * Used internally by specific subsystem creation methods.
     */
    template <typename SType, typename... Args>
    void create_subsystem_internal(SubsystemType type, Args&&... args)
    {
        auto subsystem = std::make_shared<SType>(std::forward<Args>(args)...);
        add_subsystem(type, std::move(subsystem));
    }

    /**
     * @brief Create and register the audio subsystem
     * @tparam Args Constructor argument types
     * @param args Constructor arguments for AudioSubsystem
     *
     * Specialized creation method for AudioSubsystem. Only one audio subsystem
     * is allowed per manager instance.
     */
    void create_audio_subsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type);

    /**
     * @brief Create and register the graphics subsystem
     * @param graphics_config Global graphics configuration
     *
     * Specialized creation method for GraphicsSubsystem. Only one graphics
     * subsystem is allowed per manager instance.
     */
    void create_graphics_subsystem(const GlobalGraphicsConfig& graphics_config);

    /**
     * @brief Create and register the input subsystem
     * @param input_config Global input configuration
     *
     * Specialized creation method for InputSubsystem. Only one input
     * subsystem is allowed per manager instance.
     */
    void create_input_subsystem(GlobalInputConfig& input_config);

    /** @brief Start all registered subsystems in coordination */
    void start_all_subsystems();

    /** @brief Pause all subsystems */
    void pause_all_subsystems();

    /** @brief Resume all paused subsystems */
    void resume_all_subsystems();

    /**
     * @brief Get access to a specific subsystem by type
     * @param type SubsystemType enum value identifying the subsystem
     * @return Shared pointer to subsystem or nullptr if not found
     *
     * Provides access to registered subsystems for direct interaction.
     * Returns nullptr if subsystem of specified type doesn't exist.
     */
    std::shared_ptr<ISubsystem> get_subsystem(SubsystemType type);

    /**
     * @brief Get typed access to the audio subsystem
     * @return Shared pointer to AudioSubsystem or nullptr if not created
     *
     * Convenience method that automatically casts to AudioSubsystem type.
     * Equivalent to dynamic_cast on get_subsystem(SubsystemType::AUDIO).
     */
    std::shared_ptr<AudioSubsystem> get_audio_subsystem();

    /**
     * @brief Get typed access to the graphics subsystem
     * @return Shared pointer to GraphicsSubsystem or nullptr if not created
     *
     * Convenience method that automatically casts to GraphicsSubsystem type.
     * Equivalent to dynamic_cast on get_subsystem(SubsystemType::GRAPHICS).
     */
    std::shared_ptr<GraphicsSubsystem> get_graphics_subsystem();

    /**
     * @brief Get typed access to the input subsystem
     * @return Shared pointer to InputSubsystem or nullptr if not created
     *
     * Convenience method that automatically casts to InputSubsystem type.
     * Equivalent to dynamic_cast on get_subsystem(SubsystemType::INPUT).
     */
    std::shared_ptr<InputSubsystem> get_input_subsystem();

    /**
     * @brief Check if a subsystem type exists
     * @param type SubsystemType to check for existence
     * @return True if subsystem of this type is registered
     *
     * Fast existence check without retrieving the subsystem instance.
     */
    inline bool has_subsystem(SubsystemType type) const { return m_subsystems.count(type) > 0; }

    /**
     * @brief Get all currently active subsystem types
     * @return Vector of SubsystemType values for all registered subsystems
     *
     * Returns the types of all subsystems currently managed by this instance.
     */
    std::vector<SubsystemType> get_active_subsystem_types() const;

    /**
     * @brief Register a subsystem instance with the manager
     * @param type SubsystemType enum value identifying the subsystem category
     * @param subsystem Shared pointer to the subsystem instance
     *
     * Registers a pre-constructed subsystem with the manager and creates
     * its processing handle. The subsystem is initialized but not started.
     */
    void add_subsystem(SubsystemType type, const std::shared_ptr<ISubsystem>& subsystem);

    /**
     * @brief Remove and shutdown a subsystem
     * @param type SubsystemType enum value identifying the subsystem to remove
     *
     * Stops, shuts down, and removes the specified subsystem from management.
     * Cleans up associated processing handles and resources.
     */
    void remove_subsystem(SubsystemType type);

    /**
     * @brief Query operational status of all subsystems
     * @return Map of SubsystemType to boolean indicating ready/running status
     *
     * Returns the current operational status of all managed subsystems.
     * Status reflects whether each subsystem is ready for operation.
     */
    std::unordered_map<SubsystemType, std::pair<bool, bool>> query_subsystem_status() const;

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
        SubsystemTokens /*secondary_tokens*/,
        Func operation)
    {
        SubsystemTokens combined_tokens {
            .Buffer = primary_tokens.Buffer,
            .Node = primary_tokens.Node,
            .Task = primary_tokens.Task
        };

        SubsystemProcessingHandle temp_handle(
            m_buffer_manager,
            m_node_graph_manager,
            m_task_scheduler,
            m_window_manager,
            m_input_manager,
            combined_tokens);

        operation(temp_handle);
    }

    /** @brief Stop all subsystems */
    void stop();

    /** @brief Shutdown all subsystems in proper order */
    void shutdown();

    /**
     * @brief Configure cross-subsystem data access permissions
     * @param from SubsystemType that requests access
     * @param to SubsystemType that provides data
     *
     * Establishes permission for one subsystem to read data from another.
     * Required for cross-subsystem operations like audio-reactive visuals.
     */
    void allow_cross_access(SubsystemType from, SubsystemType to);

    /**
     * @brief Read data from another subsystem's buffers
     * @param requesting_type SubsystemType making the data request
     * @param target_type SubsystemType providing the data
     * @param channel Channel index to read from
     * @return Read-only span if access allowed, nullopt otherwise
     *
     * Enables controlled cross-subsystem data sharing with permission checking.
     * Used for scenarios where one subsystem needs processed data from another.
     */
    std::optional<std::span<const double>> read_cross_subsystem_buffer(
        SubsystemType requesting_type,
        SubsystemType target_type,
        uint32_t channel);

    /**
     * @brief Register a processing hook for a specific subsystem
     * @param type SubsystemType to attach the hook to
     * @param name Unique identifier for the hook
     * @param hook Callback function to execute
     * @param position When to execute the hook (PRE_PROCESS or POST_PROCESS)
     *
     * Process hooks allow custom code execution at specific points in the
     * processing cycle. Used for monitoring, debugging, or additional processing.
     */
    void register_process_hook(SubsystemType type, const std::string& name, ProcessHook hook, HookPosition position = HookPosition::POST_PROCESS);

    /**
     * @brief Remove a previously registered processing hook
     * @param type SubsystemType the hook is attached to
     * @param name Unique identifier of the hook to remove
     *
     * Removes the named hook from the specified subsystem's processing cycle.
     */
    void unregister_process_hook(SubsystemType type, const std::string& name);

    /**
     * @brief Check if a processing hook exists
     * @param type SubsystemType to check
     * @param name Hook identifier to look for
     * @return True if hook exists (in either pre or post position)
     *
     * Checks both pre-process and post-process hook collections for the named hook.
     */
    bool has_process_hook(SubsystemType type, const std::string& name);

    /**
     * @brief Get processing handle with validation
     * @param type Subsystem type
     * @return Valid handle or nullptr if validation fails
     */
    SubsystemProcessingHandle* get_validated_handle(SubsystemType type) const;

    /** @brief Stop the audio subsystem */
    void stop_audio_subsystem();

    /** @brief Stop the graphics subsystem */
    void stop_graphics_subsystem();

    /** @brief Stop the input subsystem */
    void stop_input_subsystem();

private:
    bool is_cross_access_allowed(SubsystemType from, SubsystemType to) const;

    SubsystemTokens get_tokens_for_type(SubsystemType type) const;

    std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager;
    std::shared_ptr<Buffers::BufferManager> m_buffer_manager;
    std::shared_ptr<Vruta::TaskScheduler> m_task_scheduler;
    std::shared_ptr<Core::WindowManager> m_window_manager;
    std::shared_ptr<InputManager> m_input_manager;

    std::unordered_map<SubsystemType, std::shared_ptr<ISubsystem>> m_subsystems;
    std::unordered_map<SubsystemType, std::unique_ptr<SubsystemProcessingHandle>> m_handles;
    std::unordered_map<SubsystemType, std::unordered_set<SubsystemType>> m_cross_access_permissions;

    mutable std::shared_mutex m_mutex; ///< Thread safety for subsystem operations
};

}
