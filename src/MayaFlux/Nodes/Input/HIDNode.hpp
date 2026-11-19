#pragma once

#include "InputNode.hpp"

#include "MayaFlux/Core/Input/HIDDescriptor.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @struct HIDControl
 * @brief Addresses one control of a HID device by usage.
 *
 * Instance counts the fields of the same usage page and usage in descriptor
 * order, so it is zero unless a device repeats a usage. The Button page
 * numbers its usages from one, so a button index is its usage.
 */
struct HIDControl {
    uint16_t usage_page {};
    uint16_t usage {};
    uint16_t instance {};

    /** @brief Button page control, indexed from one as the HID spec numbers it. */
    static constexpr HIDControl button(uint16_t index) noexcept { return { 0x09, index, 0 }; }

    /** @brief Generic Desktop page control. */
    static constexpr HIDControl generic_desktop(uint16_t usage, uint16_t instance = 0) noexcept
    {
        return { 0x01, usage, instance };
    }

    static constexpr HIDControl x() noexcept { return generic_desktop(0x30); }
    static constexpr HIDControl y() noexcept { return generic_desktop(0x31); }
    static constexpr HIDControl z() noexcept { return generic_desktop(0x32); }
    static constexpr HIDControl rx() noexcept { return generic_desktop(0x33); }
    static constexpr HIDControl ry() noexcept { return generic_desktop(0x34); }
    static constexpr HIDControl rz() noexcept { return generic_desktop(0x35); }
    static constexpr HIDControl slider() noexcept { return generic_desktop(0x36); }
    static constexpr HIDControl dial() noexcept { return generic_desktop(0x37); }
    static constexpr HIDControl wheel() noexcept { return generic_desktop(0x38); }
    static constexpr HIDControl hat() noexcept { return generic_desktop(0x39); }

    /**
     * @brief A control computed by a derivation, named by the caller.
     *
     * Derived controls live on usage page zero, which the HID usage tables
     * reserve, so they never collide with a field of a device.
     */
    static constexpr HIDControl derived(uint16_t id) noexcept { return { 0x00, id, 0 }; }

    [[nodiscard]] constexpr uint64_t key() const noexcept
    {
        return (static_cast<uint64_t>(usage_page) << 32U)
            | (static_cast<uint64_t>(usage) << 16U)
            | static_cast<uint64_t>(instance);
    }

    [[nodiscard]] static constexpr HIDControl from_key(uint64_t key) noexcept
    {
        return { static_cast<uint16_t>(key >> 32U), static_cast<uint16_t>(key >> 16U), static_cast<uint16_t>(key) };
    }

    constexpr bool operator==(const HIDControl&) const noexcept = default;
};

/**
 * @enum HIDRest
 * @brief Where a control sits when nothing touches it.
 */
enum class HIDRest : uint8_t {
    MIN, ///< At the low end of its range, output 0 to 1
    CENTER ///< At the middle of its range, output -1 to 1
};

/**
 * @struct HIDControlSpec
 * @brief How a control's raw value becomes its output.
 *
 * The default maps the descriptor's logical range to 0 to 1 with no deadzone
 * and no inversion. Anything else is chosen explicitly, since a descriptor
 * does not say whether a control rests at its minimum or its middle. A raw
 * value outside the logical range, which is how a hat reports no direction,
 * reads as the rest value unless a range override is given.
 */
struct HIDControlSpec {
    HIDRest rest { HIDRest::MIN };
    double deadzone {};
    bool invert {};
    std::optional<std::pair<double, double>> range;

    /** @brief Control resting at the middle of its range. */
    static HIDControlSpec centered(double deadzone = 0.0)
    {
        HIDControlSpec spec;
        spec.rest = HIDRest::CENTER;
        spec.deadzone = deadzone;
        return spec;
    }

    HIDControlSpec with_deadzone(double value) const
    {
        HIDControlSpec copy = *this;
        copy.deadzone = value;
        return copy;
    }

    HIDControlSpec inverted() const
    {
        HIDControlSpec copy = *this;
        copy.invert = !invert;
        return copy;
    }

    HIDControlSpec with_range(double min_raw, double max_raw) const
    {
        HIDControlSpec copy = *this;
        copy.range = std::pair { min_raw, max_raw };
        return copy;
    }
};

/**
 * @struct HIDStick
 * @brief Two controls read as one stick with a radial deadzone.
 *
 * Both axes are treated as centred. The deadzone is a fraction of the full
 * radius, applied to the pair together, so the output of each axis reflects
 * the stick's distance from centre and not its own.
 */
struct HIDStick {
    HIDControl x;
    HIDControl y;
    double deadzone {};
};

/**
 * @struct HIDSnapshot
 * @brief One immutable view of a device after a report.
 *
 * raw is aligned to the layout's fields. values holds each field's output
 * after its spec and any stick has been applied, followed by one value per
 * derived control. report is the report bytes as the device sent them.
 */
struct MAYAFLUX_API HIDSnapshot {
    std::shared_ptr<const Core::HIDReportLayout> layout;
    std::vector<int32_t> raw;
    std::vector<uint8_t> report;
    std::vector<double> values;
    std::vector<uint64_t> derived_keys;

    /** @brief Index into values of a control, if the device or the config has it. */
    [[nodiscard]] std::optional<size_t> find(HIDControl control) const;

    /** @brief Output of a control, or nothing if it does not exist. */
    [[nodiscard]] std::optional<double> value(HIDControl control) const;
};

/**
 * @brief A function from device state to one control's output.
 *
 * Runs on the thread that delivers input, after every field has been
 * converted. It sees the converted fields, the raw integers, the report bytes
 * and the derived controls declared before it.
 */
using HIDDerivation = std::function<double(const HIDSnapshot&)>;

/**
 * @struct HIDConfig
 * @brief Configuration for HIDNode.
 *
 * A node is either per device or per control. HIDConfig::device() makes a node
 * with no scalar of its own, read through value(), events and signal().
 * HIDConfig::control(c) makes a node whose scalar output is control c. Both
 * are the same node: a control node can still offer events and signals.
 *
 * A config is a plain value. A profile for a particular device is a function
 * returning one, optionally with named HIDControl constants beside it, and
 * profiles combine with with().
 */
struct HIDConfig : InputConfig {
    std::optional<HIDControl> primary;
    std::vector<std::pair<HIDControl, HIDControlSpec>> specs;
    std::vector<HIDStick> sticks;
    std::vector<std::pair<HIDControl, HIDDerivation>> derived;

    /** @brief A node for the whole device. */
    static HIDConfig device() { return {}; }

    /**
     * @brief A node whose scalar output is one control.
     *
     * Button page controls disable smoothing, since an interpolated button
     * is not a meaningful quantity.
     */
    static HIDConfig control(HIDControl control)
    {
        HIDConfig cfg;
        cfg.primary = control;
        if (control.usage_page == 0x09) {
            cfg.smoothing = SmoothingMode::NONE;
        }
        return cfg;
    }

    HIDConfig with_spec(HIDControl control, HIDControlSpec spec) const
    {
        HIDConfig copy = *this;
        copy.specs.emplace_back(control, spec);
        return copy;
    }

    HIDConfig with_stick(HIDControl x, HIDControl y, double deadzone = 0.0) const
    {
        HIDConfig copy = *this;
        copy.sticks.push_back({ x, y, deadzone });
        return copy;
    }

    /**
     * @brief Add a control computed from the device state.
     *
     * The control is addressed as HIDControl::derived(id) and behaves like any
     * other: value(), events, signal() and primary all accept it. This is how
     * a field with no usage, a vendor report, a hat direction or a combination
     * of controls is exposed.
     */
    HIDConfig derive(uint16_t id, HIDDerivation derivation) const
    {
        HIDConfig copy = *this;
        copy.derived.emplace_back(HIDControl::derived(id), std::move(derivation));
        return copy;
    }

    /**
     * @brief A node whose scalar output is computed from the device state.
     *
     * The derivation sees the report bytes, so byte offsets and arbitrary
     * parsing remain possible on any device, with or without a descriptor.
     */
    template <typename F>
    static HIDConfig custom(F&& derivation)
    {
        return HIDConfig {}.derive(0, std::forward<F>(derivation)).with_primary(HIDControl::derived(0));
    }

    HIDConfig with_primary(HIDControl control) const
    {
        HIDConfig copy = *this;
        copy.primary = control;
        return copy;
    }

    /**
     * @brief Combine with another config, such as a device profile.
     *
     * Specs, sticks and derivations are appended. The other config's primary
     * replaces this one's when it has one. Derivation ids must not repeat
     * between the two.
     */
    [[nodiscard]] HIDConfig with(const HIDConfig& other) const
    {
        HIDConfig copy = *this;
        copy.specs.insert(copy.specs.end(), other.specs.begin(), other.specs.end());
        copy.sticks.insert(copy.sticks.end(), other.sticks.begin(), other.sticks.end());
        copy.derived.insert(copy.derived.end(), other.derived.begin(), other.derived.end());
        if (other.primary) {
            copy.primary = other.primary;
        }
        return copy;
    }
};

/**
 * @struct HIDControlInfo
 * @brief One control a device exposes.
 */
struct HIDControlInfo {
    HIDControl control;
    int32_t logical_min {};
    int32_t logical_max {};
    uint32_t bit_size {};
    bool relative {};
};

/**
 * @brief List the controls a layout exposes, in descriptor order.
 *
 * Array fields are left out, since their usages are not carried by the field.
 */
[[nodiscard]] MAYAFLUX_API std::vector<HIDControlInfo> hid_controls(const Core::HIDReportLayout& layout);

/**
 * @class HIDContext
 * @brief Context for HIDNode callbacks.
 *
 * Carries the whole device state alongside the control that fired, so a
 * callback on one control can read any other from the same report. For a
 * stick, value is x and y is y.
 */
class MAYAFLUX_API HIDContext : public InputContext {
public:
    HIDContext(double value, double raw_value, Core::InputType source, uint32_t device_id)
        : InputContext(value, raw_value, source, device_id)
    {
    }

    std::shared_ptr<const HIDSnapshot> snapshot;
    HIDControl control;
    double y {};
};

/**
 * @class HIDNode
 * @brief InputNode over the decoded state of one HID device.
 *
 * Receives InputValue::Type::HID from HIDBackend, which carries every field of
 * the device by usage, and the report bytes for anything the descriptor does
 * not describe. The class is open for derivation: a device specific node is a
 * subclass whose constructor passes a profile config and whose accessors call
 * value() with named HIDControl constants. It registers like any InputNode.
 *
 * Output comes three ways, all reading the same converted values:
 * - the node's own scalar, when the config names a primary control;
 * - value(), and events keyed by control, on the device node;
 * - signal(), a plain InputNode for one control, for wiring into a graph.
 *
 * Only the device node is registered with the InputManager. Signals are fed
 * from it, on the same thread, so any number of them cost one registration.
 *
 * @code
 * auto pad = vega.read_hid(HIDConfig::device()
 *                              .with_spec(HIDControl::z(), HIDControlSpec::centered(0.1)),
 *     bind_hid(id));
 *
 * pad->on_press(HIDControl::button(1), [](HIDContext& ctx) { });
 * auto z = pad->signal(HIDControl::z());
 *
 * auto fader = vega.read_hid(HIDConfig::control(HIDControl::slider()), bind_hid(id));
 * @endcode
 */
class MAYAFLUX_API HIDNode : public InputNode {
public:
    using EventCallback = TypedHook<HIDContext>;

    explicit HIDNode(HIDConfig config = {});

    void save_state() override { }
    void restore_state() override { }

    NodeContext& get_last_context() override { return m_hid_context; }

    /** @brief Fires when a control's output rises through threshold. */
    HookId on_press(HIDControl control, EventCallback callback, double threshold = 0.5);

    /** @brief Fires when a control's output falls through threshold. */
    HookId on_release(HIDControl control, EventCallback callback, double threshold = 0.5);

    /** @brief Fires when a control's output moves by more than epsilon. */
    HookId on_change(HIDControl control, EventCallback callback, double epsilon = 0.0001);

    /** @brief Fires when a hat control's direction changes, including to none. */
    HookId on_hat(HIDControl control, EventCallback callback);

    /** @brief Fires when a configured stick, named by its x control, moves. */
    HookId on_stick(HIDControl x, EventCallback callback);

    /** @brief Fires on every report that updates the device. */
    HookId on_frame(EventCallback callback);

    /**
     * @brief A scalar node for one control.
     *
     * Created on first request and returned again for the same control. It is
     * fed by this node and is not registered anywhere, so it goes quiet if
     * this node is unregistered.
     */
    [[nodiscard]] std::shared_ptr<InputNode> signal(HIDControl control);

    /** @brief Output of a control as of the last report, or the default value. */
    [[nodiscard]] double value(HIDControl control) const;

    /**
     * @brief Direction of a hat control, 0 to 7 clockwise from the first position.
     * @return Nothing when the hat reports no direction or the device lacks it.
     */
    [[nodiscard]] std::optional<uint8_t> hat(HIDControl control) const;

    /** @brief Last device state, or null before the first report. */
    [[nodiscard]] std::shared_ptr<const HIDSnapshot> get_snapshot() const;

    /** @brief Controls of the device, empty before the first report. */
    [[nodiscard]] std::vector<HIDControlInfo> controls() const;

protected:
    double extract_value(const Core::InputValue& value) override;

    void update_context(double value) override;

    void notify_tick(double value) override;

private:
    enum class EventKind : uint8_t {
        PRESS,
        RELEASE,
        CHANGE,
        HAT,
        STICK,
        FRAME
    };

    struct ResolvedStick {
        HIDControl control;
        size_t x {};
        size_t y {};
        double deadzone {};
    };

    struct SignalEntry {
        uint64_t key {};
        std::shared_ptr<InputNode> node;
    };

    HIDConfig m_config;
    HIDContext m_hid_context;

    Memory::SnapshotCell<std::shared_ptr<const HIDSnapshot>> m_snapshot;
    Memory::SnapshotCell<std::vector<SignalEntry>> m_signals;
    HookList m_event_callbacks { *this };

    std::shared_ptr<const Core::HIDReportLayout> m_layout;
    std::unordered_map<uint64_t, size_t> m_lookup;
    std::vector<HIDControlSpec> m_specs;
    std::vector<ResolvedStick> m_resolved_sticks;
    std::optional<size_t> m_primary;

    std::shared_ptr<const HIDSnapshot> m_current;
    std::vector<double> m_previous;
    std::vector<int32_t> m_previous_raw;
    bool m_have_previous { false };
    bool m_pending { false };
    uint32_t m_device_id {};

    HookId add_event(EventKind kind, HIDControl control, EventCallback callback, double parameter);

    void resolve(const std::shared_ptr<const Core::HIDReportLayout>& layout);
    [[nodiscard]] std::vector<double> convert(const Core::InputValue::HIDFrame& frame) const;
    void feed_signals();
    void fire_events();
};

} // namespace MayaFlux::Nodes::Input
