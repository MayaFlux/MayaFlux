#pragma once

#include "InputNode.hpp"

#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @enum MIDIKind
 * @brief The kinds of channel message a MIDI control can be.
 */
enum class MIDIKind : uint8_t {
    NOTE, ///< Note on and note off, number is the note
    CC, ///< Control change, number is the controller
    PITCH_BEND, ///< Pitch bend, one per channel
    PRESSURE, ///< Channel pressure, one per channel
    PROGRAM ///< Program change, one per channel
};

/**
 * @struct MIDIControl
 * @brief Names a set of MIDI messages: one kind, optionally one number and one channel.
 *
 * An empty number or channel matches any. Notes and control changes are
 * numbered, the other kinds have no number. Channels are 0 to 15.
 */
struct MIDIControl {
    MIDIKind kind { MIDIKind::NOTE };
    std::optional<uint8_t> number;
    std::optional<uint8_t> channel;

    static constexpr MIDIControl note(std::optional<uint8_t> note = {}, std::optional<uint8_t> channel = {}) noexcept
    {
        return { MIDIKind::NOTE, note, channel };
    }

    static constexpr MIDIControl cc(std::optional<uint8_t> controller = {}, std::optional<uint8_t> channel = {}) noexcept
    {
        return { MIDIKind::CC, controller, channel };
    }

    static constexpr MIDIControl pitch_bend(std::optional<uint8_t> channel = {}) noexcept
    {
        return { MIDIKind::PITCH_BEND, {}, channel };
    }

    static constexpr MIDIControl pressure(std::optional<uint8_t> channel = {}) noexcept
    {
        return { MIDIKind::PRESSURE, {}, channel };
    }

    static constexpr MIDIControl program(std::optional<uint8_t> channel = {}) noexcept
    {
        return { MIDIKind::PROGRAM, {}, channel };
    }

    /** @brief The kind a status byte carries, or nothing for system messages and poly pressure. */
    [[nodiscard]] static constexpr std::optional<MIDIKind> kind_of(uint8_t status) noexcept
    {
        switch (status & 0xF0U) {
        case 0x80U:
        case 0x90U:
            return MIDIKind::NOTE;
        case 0xB0U:
            return MIDIKind::CC;
        case 0xE0U:
            return MIDIKind::PITCH_BEND;
        case 0xD0U:
            return MIDIKind::PRESSURE;
        case 0xC0U:
            return MIDIKind::PROGRAM;
        default:
            return std::nullopt;
        }
    }

    /** @brief Whether a message belongs to this control. */
    [[nodiscard]] bool matches(const Core::InputValue::MIDIMessage& midi) const noexcept
    {
        const auto message_kind = kind_of(midi.status);
        if (!message_kind || *message_kind != kind) {
            return false;
        }
        if (channel && *channel != midi.channel()) {
            return false;
        }
        const bool numbered = kind == MIDIKind::NOTE || kind == MIDIKind::CC;
        return !(numbered && number && *number != midi.data1);
    }
};

/**
 * @brief MIDI input node configuration
 *
 * The filters decide which messages the node takes in. A message the filters
 * reject changes nothing: not the scalar, not the state, and no callback fires.
 * A config with no filter takes every channel message, which makes a node for a
 * whole device.
 */
struct MIDIConfig : InputConfig {
    std::optional<uint8_t> channel; ///< MIDI channel filter (0-15)
    std::optional<uint8_t> note_number; ///< Note number filter (0-127)
    std::optional<uint8_t> cc_number; ///< CC number filter (0-127)
    std::optional<uint8_t> message_type; ///< Message type filter (0x80-0xF0)

    bool note_on_only {}; ///< Only respond to Note On
    bool note_off_only {}; ///< Only respond to Note Off

    std::function<double(uint8_t)> velocity_curve; ///< Custom velocity mapping
    bool invert_cc { false }; ///< Flip CC 127→0

    /**
     * @brief A node for the whole device: every channel message is taken in.
     *
     * Read it through value accessors, keyed events and signals.
     */
    static MIDIConfig device() { return {}; }

    /**
     * @brief Note velocity node (responds to any note)
     */
    static MIDIConfig note() { return {}; }

    /**
     * @brief Specific note velocity
     */
    static MIDIConfig note(uint8_t note_num)
    {
        MIDIConfig cfg;
        cfg.note_number = note_num;
        return cfg;
    }

    /**
     * @brief Control Change node
     */
    static MIDIConfig cc(uint8_t cc_num)
    {
        MIDIConfig cfg;
        cfg.cc_number = cc_num;
        cfg.message_type = 0xB0;
        return cfg;
    }

    /**
     * @brief Pitch bend node
     */
    static MIDIConfig pitch_bend()
    {
        MIDIConfig cfg;
        cfg.message_type = 0xE0;
        return cfg;
    }

    /**
     * @brief Aftertouch node
     */
    static MIDIConfig aftertouch()
    {
        MIDIConfig cfg;
        cfg.message_type = 0xD0;
        return cfg;
    }

    /**
     * @brief Program change node
     */
    static MIDIConfig program_change()
    {
        MIDIConfig cfg;
        cfg.message_type = 0xC0;
        return cfg;
    }

    /**
     * @brief Apply velocity curve transformation
     */
    template <typename F>
    MIDIConfig& with_velocity_curve(F&& curve)
    {
        velocity_curve = std::forward<F>(curve);
        return *this;
    }

    MIDIConfig& on_channel(uint8_t ch)
    {
        channel = ch;
        return *this;
    }

    MIDIConfig& note_on()
    {
        note_on_only = true;
        return *this;
    }

    MIDIConfig& note_off()
    {
        note_off_only = true;
        return *this;
    }

    MIDIConfig& inverted()
    {
        invert_cc = true;
        return *this;
    }
};

/**
 * @class MIDIContext
 * @brief Context for MIDINode callbacks, describing the message that fired them.
 *
 * value is the scalar the message maps to. data is the velocity of a note, the
 * value of a control change, or the pressure or program number.
 */
class MAYAFLUX_API MIDIContext : public InputContext {
public:
    MIDIContext(double value, double raw_value, Core::InputType source, uint32_t device_id)
        : InputContext(value, raw_value, source, device_id)
    {
    }

    MIDIKind kind { MIDIKind::NOTE };
    uint8_t channel {};
    uint8_t number {}; ///< Note or controller number, zero for other kinds
    uint8_t data {};
    int16_t bend {}; ///< Pitch bend, -8192 to 8191
    bool is_on {}; ///< A note on with a velocity above zero
};

/**
 * @class MIDINode
 * @brief InputNode over MIDI channel messages, per control or per device.
 *
 * A message names its own control, as kind, channel and number, so a MIDI
 * device has no layout to discover and a controller uses an unknown part of
 * a large space. The node takes two shapes over the same machinery:
 * - per control, through a filtered config such as MIDIConfig::cc(1): the
 *   scalar is that control and the callbacks fire only for it;
 * - per device, through MIDIConfig::device(): the node keeps the state of every
 *   control, offers keyed events through on(), and hands out signal() nodes
 *   for the few controls wired into a graph.
 *
 * Notes are voices and not signals, so they are read through events and the
 * held note state, never as one scalar per key. Continuous controls, which
 * are control changes, pitch bend and channel pressure, are signals.
 *
 * Example usage:
 * @code
 * // Per control
 * auto mod_wheel = vega.read_midi(MIDIConfig::cc(1), binding);
 *
 * // Per device
 * auto keys = vega.read_midi(MIDIConfig::device(), binding);
 * keys->on(MIDIControl::note(), [](MIDIContext& ctx) {
 *     // every note on either channel, with ctx.number and ctx.data
 * });
 * auto cutoff = keys->signal(MIDIControl::cc(74, 0));
 * double bend = keys->bend_value(0);
 * auto held = keys->held_notes(0);
 * @endcode
 */
class MAYAFLUX_API MIDINode : public InputNode {
public:
    using NoteCallback = std::function<void(uint8_t note, uint8_t velocity, bool is_on)>;
    using CC_Callback = std::function<void(uint8_t cc_num, uint8_t value)>;
    using PitchBendCallback = std::function<void(int16_t bend)>;
    using EventCallback = TypedHook<MIDIContext>;

    explicit MIDINode(MIDIConfig config = {});

    void save_state() override { }
    void restore_state() override { }

    NodeContext& get_last_context() override { return m_midi_context; }

    /**
     * @brief Callback for messages that belong to a control
     *
     * Fires for every message the node takes in that the control matches. For a
     * note it fires on both note on and note off, and ctx.is_on tells which.
     */
    HookId on(MIDIControl control, EventCallback callback);

    /**
     * @brief A scalar node for one control.
     *
     * Created on first request and returned again for the same control. It is
     * fed by this node whenever a message matches, and is not registered
     * anywhere, so it goes quiet if this node is unregistered. Notes and
     * program changes disable smoothing.
     */
    [[nodiscard]] std::shared_ptr<InputNode> signal(MIDIControl control);

    /** @brief Last control change value on a channel, 0 to 1. */
    [[nodiscard]] double cc_value(uint8_t channel, uint8_t controller) const;

    /** @brief Velocity of a held note, 0 to 127, and zero when it is not held. */
    [[nodiscard]] uint8_t velocity_of(uint8_t channel, uint8_t note) const;

    /** @brief Whether a note is currently held. */
    [[nodiscard]] bool note_held(uint8_t channel, uint8_t note) const;

    /** @brief The notes held on a channel, ascending. */
    [[nodiscard]] std::vector<uint8_t> held_notes(uint8_t channel) const;

    /** @brief Pitch bend on a channel, -1 to 1. */
    [[nodiscard]] double bend_value(uint8_t channel) const;

    /** @brief Channel pressure on a channel, 0 to 1. */
    [[nodiscard]] double pressure_value(uint8_t channel) const;

    /** @brief Last program change on a channel. */
    [[nodiscard]] uint8_t program_of(uint8_t channel) const;

    /**
     * @brief Callback for note events with note number and velocity
     * @param callback Function receiving (note_number, velocity, is_note_on)
     */
    HookId on_note(std::function<void(uint8_t note, uint8_t velocity, bool is_on)> callback)
    {
        return m_note_callbacks.add(
            [this, callback](NodeContext&) {
                if (!m_last_midi_message) {
                    return;
                }
                const auto& midi = *m_last_midi_message;
                if (midi.type() == 0x90) {
                    callback(midi.data1, midi.data2, midi.data2 > 0);
                } else {
                    callback(midi.data1, 0, false);
                }
            },
            callback.target_type());
    }

    /**
     * @brief Callback for CC events with controller number and value
     */
    HookId on_cc(std::function<void(uint8_t cc_num, uint8_t value)> callback)
    {
        return m_cc_callbacks.add(
            [this, callback](NodeContext&) {
                if (m_last_midi_message) {
                    callback(m_last_midi_message->data1, m_last_midi_message->data2);
                }
            },
            callback.target_type());
    }

    /**
     * @brief Callback for pitch bend with 14-bit value
     */
    HookId on_pitch_bend(std::function<void(int16_t bend)> callback)
    {
        return m_pitch_bend_callbacks.add(
            [this, callback](NodeContext&) {
                if (m_last_midi_message) {
                    callback(static_cast<int16_t>(((m_last_midi_message->data2 << 7) | m_last_midi_message->data1) - 8192));
                }
            },
            callback.target_type());
    }

protected:
    double extract_value(const Core::InputValue& value) override;

    void update_context(double value) override;

    void notify_tick(double value) override;

private:
    struct State {
        std::array<std::array<std::atomic<uint8_t>, 128>, 16> cc {};
        std::array<std::array<std::atomic<uint8_t>, 128>, 16> velocity {};
        std::array<std::atomic<int16_t>, 16> bend {};
        std::array<std::atomic<uint8_t>, 16> pressure {};
        std::array<std::atomic<uint8_t>, 16> program {};
    };

    struct SignalEntry {
        MIDIControl control;
        std::shared_ptr<InputNode> node;
    };

    MIDIConfig m_config;
    MIDIContext m_midi_context;
    std::optional<Core::InputValue::MIDIMessage> m_last_midi_message;
    bool m_accepted { false };
    uint32_t m_device_id {};

    State m_state;
    Memory::SnapshotCell<std::vector<SignalEntry>> m_signals;

    HookList m_note_callbacks { *this };
    HookList m_cc_callbacks { *this };
    HookList m_pitch_bend_callbacks { *this };
    HookList m_event_callbacks { *this };

    void fire_midi_callbacks(const Core::InputValue::MIDIMessage& midi);
    void fire_keyed_events(const Core::InputValue::MIDIMessage& midi);
    void feed_signals(const Core::InputValue::MIDIMessage& midi);
    void record_state(const Core::InputValue::MIDIMessage& midi);
    void describe(const Core::InputValue::MIDIMessage& midi);

    [[nodiscard]] bool accepts(const Core::InputValue::MIDIMessage& midi) const;
    [[nodiscard]] double scalar_of(const Core::InputValue::MIDIMessage& midi) const;
};

} // namespace MayaFlux::Nodes::Input
