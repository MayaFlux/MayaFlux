#pragma once

#include "MayaFlux/Core/Backends/Input/InputBackend.hpp"
#include "MayaFlux/Core/Subsystems/Subsystem.hpp"

namespace MayaFlux::Nodes::Input {
class InputNode;
}

namespace MayaFlux::Core {

class InputManager;
struct InputBinding;

/**
 * @class InputSubsystem
 * @brief Input processing subsystem for external devices
 *
 * Coordinates input backends (HID, MIDI, OSC, Serial) and InputManager.
 * Follows the same lifecycle patterns as AudioSubsystem and GraphicsSubsystem.
 *
 * Responsibilities:
 * - Owns and manages input backends based on GlobalInputConfig
 * - Owns InputManager which handles processing thread and node dispatch
 * - Routes backend callbacks to InputManager's queue
 * - Provides node registration API (delegates to InputManager)
 *
 * Does NOT directly call process_sample on nodes - that's InputManager's job.
 */
class MAYAFLUX_API InputSubsystem : public ISubsystem {
public:
    explicit InputSubsystem(GlobalInputConfig& config);
    ~InputSubsystem() override;

    // Non-copyable, non-movable
    InputSubsystem(const InputSubsystem&) = delete;
    InputSubsystem& operator=(const InputSubsystem&) = delete;
    InputSubsystem(InputSubsystem&&) = delete;
    InputSubsystem& operator=(InputSubsystem&&) = delete;

    // ─────────────────────────────────────────────────────────────────────
    // ISubsystem Implementation
    // ─────────────────────────────────────────────────────────────────────

    void register_callbacks() override;
    void initialize(SubsystemProcessingHandle& handle) override;
    void start() override;
    void pause() override;
    void resume() override;
    void stop() override;
    void shutdown() override;

    [[nodiscard]] SubsystemTokens get_tokens() const override { return m_tokens; }
    [[nodiscard]] bool is_ready() const override { return m_ready.load(); }
    [[nodiscard]] bool is_running() const override { return m_running.load(); }
    [[nodiscard]] SubsystemType get_type() const override { return SubsystemType::CUSTOM; }
    SubsystemProcessingHandle* get_processing_context_handle() override { return m_handle; }

    // ─────────────────────────────────────────────────────────────────────
    // Backend Management
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Add a custom input backend
     * @param backend Backend instance (takes ownership)
     * @return true if added successfully
     */
    bool add_backend(std::unique_ptr<IInputBackend> backend);

    /**
     * @brief Get a backend by type
     */
    [[nodiscard]] IInputBackend* get_backend(InputType type) const;

    /**
     * @brief Get all active backends
     */
    [[nodiscard]] std::vector<IInputBackend*> get_backends() const;

    // ─────────────────────────────────────────────────────────────────────
    // Device Management
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get all available input devices across all backends
     */
    [[nodiscard]] std::vector<InputDeviceInfo> get_all_devices() const;

    /**
     * @brief Open a device
     */
    bool open_device(InputType backend_type, uint32_t device_id);

    /**
     * @brief Close a device
     */
    void close_device(InputType backend_type, uint32_t device_id);

private:
    GlobalInputConfig& m_config;
    SubsystemProcessingHandle* m_handle { nullptr };
    SubsystemTokens m_tokens;

    std::atomic<bool> m_ready { false };
    std::atomic<bool> m_running { false };

    mutable std::shared_mutex m_backends_mutex;
    std::unordered_map<InputType, std::unique_ptr<IInputBackend>> m_backends;

    void initialize_hid_backend();
    void initialize_midi_backend();
    void initialize_osc_backend();
    void initialize_serial_backend();

    void wire_backend_to_manager(IInputBackend* backend);
};

} // namespace MayaFlux::Core
