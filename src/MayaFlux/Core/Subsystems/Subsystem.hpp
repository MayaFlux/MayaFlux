#pragma once
#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Core {

/** * @struct SubsystemTokens
 * @brief Contains processing tokens for the subsystem's buffers and nodes
 *
 * This structure holds the processing tokens used by the subsystem for its
 * buffer and node management. It allows subsystems to define their own
 * processing characteristics and ensures that they can be processed correctly
 * within the MayaFlux framework.
 */
struct SubsystemTokens {
    /** * @brief Processing token for buffers in this subsystem
     *
     * This token defines how buffers are processed, including rate, device,
     * and concurrency characteristics. It is used to ensure that buffer
     * operations are compatible with the subsystem's processing model.
     */
    MayaFlux::Buffers::ProcessingToken Buffer;

    /** * @brief Processing token for nodes in this subsystem
     *
     * This token defines how nodes are processed within the subsystem,
     * including their processing rate and execution characteristics.
     * It is used to ensure that node operations align with the subsystem's
     * processing model and can be executed correctly.
     */
    MayaFlux::Nodes::ProcessingToken Node;
};
}

namespace std {
template <>
struct hash<MayaFlux::Core::SubsystemTokens> {
    size_t operator()(const MayaFlux::Core::SubsystemTokens& tokens) const noexcept
    {
        return hash<int>()(static_cast<int>(tokens.Node)) ^ hash<int>()(static_cast<int>(tokens.Buffer));
    }
};
}

namespace MayaFlux::Core {
class ISubsystem {
public:
    // virtual ~ISubsystem() = default;

    virtual void initialize(
        std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
        std::shared_ptr<Buffers::BufferManager> buffer_manager)
        = 0;

    /**
     * @brief Register callback hooks for this domain
     *
     * Sets up domain-specific callbacks that will trigger token-based processing.
     * For audio: RtAudio callback registration
     * For visual: Vulkan present callback / OpenFrameworks draw loop
     * For windowing: GLFW event loops
     */
    virtual void register_callbacks() = 0;

    /**
     * @brief Start the subsystem's processing/event loops
     */
    virtual void start() = 0;

    /**
     * @brief Stop the subsystem's processing/event loops
     */
    virtual void stop() = 0;

    /**
     * @brief Get the processing token this subsystem manages
     */
    virtual SubsystemTokens get_subsystem_tokens() const = 0;

    /**
     * @brief Check if subsystem is ready for operation
     */
    virtual bool is_ready() const = 0;

    /**
     * @brief Shutdown and cleanup subsystem resources
     */
    virtual void shutdown() = 0;

    // private:
    //     ISubsystem() = default;
};
}
