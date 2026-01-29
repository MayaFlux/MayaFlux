#pragma once
#include "MayaFlux/Core/ProcessingArchitecture.hpp"

namespace MayaFlux::Core {

enum class SubsystemType : uint8_t {
    AUDIO,
    GRAPHICS,
    INPUT,
    NETWORK,
    CUSTOM
};

/**
 * @class ISubsystem
 * @brief Base interface for all subsystems in the MayaFlux processing architecture
 *
 * Defines the standard lifecycle and integration pattern for subsystems that
 * participate in token-based multimodal processing. Each subsystem manages
 * a specific processing domain (audio, video, custom) and coordinates with
 * the central processing architecture through standardized interfaces.
 *
 * Subsystems follow a consistent lifecycle: register_callbacks() -> initialize()
 * -> start() -> [processing] -> stop() -> shutdown()
 */
class MAYAFLUX_API ISubsystem {
public:
    virtual ~ISubsystem() = default;

    /**
     * @brief Register callback hooks for this domain
     *
     * Sets up domain-specific callbacks that will trigger token-based processing.
     * This is where subsystems connect to their respective backend systems and
     * establish the event loops that drive processing.
     *
     * Examples:
     * - Audio: RtAudio callback registration for real-time audio processing
     * - Visual: Vulkan present callback / OpenFrameworks draw loop integration
     * - Windowing: GLFW event loops for UI and input handling
     * - Custom: Application-specific timing or event-driven processing
     *
     * Called during subsystem setup before initialization. Should not start
     * actual processing - only establish the callback infrastructure.
     */
    virtual void register_callbacks() = 0;

    /**
     * @brief Initialize with a handle provided by SubsystemManager
     * @param handle Processing handle providing access to buffer and node operations
     *
     * The handle is borrowed - subsystem doesn't own it but uses it throughout
     * its lifetime. This is where subsystems configure their processing resources,
     * create initial nodes, setup buffer configurations, and prepare for operation.
     *
     * The handle provides token-scoped access to both buffer processing and node
     * graph operations, ensuring the subsystem operates within its designated
     * processing domain with proper thread safety and resource isolation.
     */
    virtual void initialize(SubsystemProcessingHandle& handle) = 0;

    /**
     * @brief Start the subsystem's processing/event loops
     *
     * Begins active processing. After this call, the subsystem should be
     * actively responding to callbacks and processing data according to
     * its domain requirements.
     */
    virtual void start() = 0;

    /** @brief Stop the subsystem's processing/event loops */
    virtual void stop() = 0;

    /** @brief Pause the subsystem's processing/event loops */
    virtual void pause() = 0;

    /** @brief Resume the subsystem's processing/event loops */
    virtual void resume() = 0;

    /**
     * @brief Get the processing token configuration this subsystem manages
     * @return SubsystemTokens defining buffer and node processing characteristics
     *
     * Returns the token configuration that defines how this subsystem processes
     * buffers and nodes. Used by the SubsystemManager for routing and coordination.
     * Should remain constant throughout the subsystem's lifetime.
     */
    [[nodiscard]] virtual SubsystemTokens get_tokens() const = 0;

    /** @brief Check if subsystem is ready for operation */
    [[nodiscard]] virtual bool is_ready() const = 0;

    /** @brief Check if subsystem is currently processing */
    [[nodiscard]] virtual bool is_running() const = 0;

    /** @brief Shutdown and cleanup subsystem resources */
    virtual void shutdown() = 0;

    /** @brief Get the type of this subsystem */
    [[nodiscard]] virtual SubsystemType get_type() const = 0;

    /** @brief Get the processing context handle for this subsystem */
    virtual SubsystemProcessingHandle* get_processing_context_handle() = 0;
};

}
