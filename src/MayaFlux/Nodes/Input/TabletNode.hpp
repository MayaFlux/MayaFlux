#pragma once

#include "InputNode.hpp"

#include "MayaFlux/Core/Input/TabletFrame.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @class TabletContext
 * @brief Node context carrying a full tablet frame alongside the scalar.
 *
 * Every callback registered on a TabletNode receives this, so a threshold
 * or range callback firing on one axis can read every other axis of the
 * same hardware sample.
 *
 * The frame is a copy rather than a view. The InputValue it came from does
 * not outlive the dispatch call, so a span would dangle.
 */
class MAYAFLUX_API TabletContext : public InputContext {
public:
    TabletContext(double value, double raw_value,
        Core::InputType source, uint32_t device_id)
        : InputContext(value, raw_value, source, device_id)
    {
    }

    std::array<double, Core::TabletFrame::SLOT_COUNT> frame {};

    /** @brief Read one slot of the frame. */
    [[nodiscard]] double slot(size_t index) const
    {
        return index < frame.size() ? frame[index] : 0.0;
    }

    /** @brief Whether the tool is within sensing range. */
    [[nodiscard]] bool in_proximity() const
    {
        return Core::TabletFrame::in_state(frame, Core::TabletState::IN_PROXIMITY);
    }

    /** @brief Whether the tool is touching the surface. */
    [[nodiscard]] bool in_contact() const
    {
        return Core::TabletFrame::in_state(frame, Core::TabletState::IN_CONTACT);
    }

    /** @brief Test a tool button by bit index. */
    [[nodiscard]] bool button(uint32_t bit) const
    {
        const auto mask = static_cast<uint32_t>(frame[Core::TabletFrame::BUTTONS]);
        return (mask & (1U << bit)) != 0U;
    }
};

/**
 * @struct TabletConfig
 * @brief Selects which part of a tablet frame becomes the node's scalar.
 *
 * One node projects one slot. Several nodes bound to the same tool receive
 * the identical frame in one dispatch pass, so their outputs stay coherent
 * without any coordination between them.
 */
struct TabletConfig : InputConfig {
    size_t slot { Core::TabletFrame::X }; ///< Frame slot driving the scalar

    std::optional<uint32_t> button_bit; ///< When set, extract this BUTTONS bit
    std::optional<Core::TabletState> state_bit; ///< When set, extract this STATE bit

    /** @brief Horizontal position, 0 to 1. */
    static TabletConfig position_x() { return {}; }

    /** @brief Vertical position, 0 to 1. */
    static TabletConfig position_y()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::Y;
        return cfg;
    }

    /** @brief Tip pressure, 0 to 1. */
    static TabletConfig pressure()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::PRESSURE;
        return cfg;
    }

    /** @brief Hover height, 0 to 1. */
    static TabletConfig distance()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::DISTANCE;
        return cfg;
    }

    /** @brief Tilt about the x axis, degrees. */
    static TabletConfig tilt_x()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::TILT_X;
        return cfg;
    }

    /** @brief Tilt about the y axis, degrees. */
    static TabletConfig tilt_y()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::TILT_Y;
        return cfg;
    }

    /** @brief Barrel rotation, degrees. */
    static TabletConfig rotation()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::ROTATION;
        return cfg;
    }

    /** @brief Barrel pressure or finger wheel, -1 to 1. */
    static TabletConfig slider()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::SLIDER;
        return cfg;
    }

    /** @brief Wheel rotation this frame, degrees. */
    static TabletConfig wheel()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::WHEEL;
        cfg.smoothing = SmoothingMode::NONE;
        return cfg;
    }

    /**
     * @brief Tip contact as 0.0 or 1.0.
     *
     * Smoothing is disabled, since an interpolated contact state is not a
     * meaningful quantity.
     */
    static TabletConfig contact()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::STATE;
        cfg.state_bit = Core::TabletState::IN_CONTACT;
        cfg.smoothing = SmoothingMode::NONE;
        return cfg;
    }

    /** @brief Proximity as 0.0 or 1.0, smoothing disabled. */
    static TabletConfig proximity()
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::STATE;
        cfg.state_bit = Core::TabletState::IN_PROXIMITY;
        cfg.smoothing = SmoothingMode::NONE;
        return cfg;
    }

    /**
     * @brief A tool button as 0.0 or 1.0, smoothing disabled.
     * @param bit Bit 0 barrel, bit 1 secondary barrel, bit 2 eraser.
     */
    static TabletConfig button(uint32_t bit)
    {
        TabletConfig cfg;
        cfg.slot = Core::TabletFrame::BUTTONS;
        cfg.button_bit = bit;
        cfg.smoothing = SmoothingMode::NONE;
        return cfg;
    }

    TabletConfig& at_slot(size_t index)
    {
        slot = index;
        return *this;
    }
};

/**
 * @class TabletNode
 * @brief InputNode projecting one slot of a tablet frame.
 *
 * Receives InputValue::Type::VECTOR frames from TabletBackend and outputs
 * the configured slot as a smoothed scalar, while carrying the whole frame
 * into every callback through TabletContext.
 *
 * Multi-axis use is several TabletNode instances bound to the same tool.
 * InputManager hands each the same InputValue in one dispatch pass, so
 * their frames are the same hardware sample.
 *
 * Contact and proximity edges fire regardless of which slot the node
 * projects, since they are transitions of the frame rather than of the
 * scalar.
 *
 * Example usage:
 * @code
 * auto tools = engine.input().get_backend(Core::InputType::TABLET)->get_devices();
 *
 * auto pressure = std::make_shared<TabletNode>(TabletConfig::pressure());
 * register_input_node(pressure, InputBinding::tablet(tools[0].id));
 *
 * pressure->on_contact_begin([](TabletContext& ctx) {
 *     // stroke start, with position and tilt available
 * });
 *
 * pressure->on_threshold_rising(0.5, [](InputContext& ctx) {
 *     // half pressure crossed
 * });
 * @endcode
 */
class MAYAFLUX_API TabletNode : public InputNode {
public:
    using FrameCallback = TypedHook<TabletContext>;

    explicit TabletNode(TabletConfig config = {});

    void save_state() override { }
    void restore_state() override { }

    NodeContext& get_last_context() override { return m_tablet_context; }

    /**
     * @brief Callback on every frame, with the full slot set.
     */
    void on_frame(FrameCallback callback)
    {
        m_frame_callbacks.push_back(std::move(callback));
    }

    /**
     * @brief Callback when the tool enters sensing range.
     */
    void on_proximity_enter(FrameCallback callback)
    {
        m_proximity_enter_callbacks.push_back(std::move(callback));
    }

    /**
     * @brief Callback when the tool leaves sensing range.
     */
    void on_proximity_exit(FrameCallback callback)
    {
        m_proximity_exit_callbacks.push_back(std::move(callback));
    }

    /**
     * @brief Callback when the tip touches the surface.
     */
    void on_contact_begin(FrameCallback callback)
    {
        m_contact_begin_callbacks.push_back(std::move(callback));
    }

    /**
     * @brief Callback when the tip leaves the surface.
     */
    void on_contact_end(FrameCallback callback)
    {
        m_contact_end_callbacks.push_back(std::move(callback));
    }

    /**
     * @brief Most recent frame received, all slots.
     */
    [[nodiscard]] std::array<double, Core::TabletFrame::SLOT_COUNT> get_frame() const
    {
        return m_tablet_context.frame;
    }

protected:
    double extract_value(const Core::InputValue& value) override;

    void update_context(double value) override;

    void notify_tick(double value) override;

private:
    TabletConfig m_config;
    TabletContext m_tablet_context;

    std::array<double, Core::TabletFrame::SLOT_COUNT> m_frame {};
    uint32_t m_previous_state {};
    std::atomic<bool> m_frame_pending { false };

    std::vector<FrameCallback> m_frame_callbacks;
    std::vector<FrameCallback> m_proximity_enter_callbacks;
    std::vector<FrameCallback> m_proximity_exit_callbacks;
    std::vector<FrameCallback> m_contact_begin_callbacks;
    std::vector<FrameCallback> m_contact_end_callbacks;

    void fire_edges();
    void fire(const std::vector<FrameCallback>& callbacks);
};

} // namespace MayaFlux::Nodes::Input
