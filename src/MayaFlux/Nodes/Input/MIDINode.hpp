#pragma once

#include "InputNode.hpp"

namespace MayaFlux::Nodes::Input {

/**
 * @brief MIDI input node configuration
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
 * @class MIDINode
 * @brief Specialized InputNode for MIDI messages
 *
 * Extracts and processes MIDI data with convenient filtering:
 * - Note numbers and velocities
 * - Control Change values
 * - Pitch bend (normalized to [-1, 1])
 * - Channel-specific filtering
 * - Message type filtering
 *
 * Example usage:
 * @code
 * // Any note velocity
 * auto note_vel = std::make_shared<MIDINode>(MIDIConfig::note());
 *
 * // Specific note on channel 1
 * auto middle_c = std::make_shared<MIDINode>(
 *     MIDIConfig::note(60).on_channel(0));
 *
 * // Mod wheel (CC 1)
 * auto mod_wheel = std::make_shared<MIDINode>(MIDIConfig::cc(1));
 *
 * // Pitch bend on channel 2
 * auto pitch = std::make_shared<MIDINode>(
 *     MIDIConfig::pitch_bend().on_channel(1));
 * @endcode
 */
class MAYAFLUX_API MIDINode : public InputNode {
public:
    using NoteCallback = std::function<void(uint8_t note, uint8_t velocity, bool is_on)>;
    using CC_Callback = std::function<void(uint8_t cc_num, uint8_t value)>;
    using PitchBendCallback = std::function<void(int16_t bend)>;

    explicit MIDINode(MIDIConfig config = {});

    void save_state() override { }
    void restore_state() override { }

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

    void notify_tick(double value) override;

private:
    MIDIConfig m_config;
    std::optional<Core::InputValue::MIDIMessage> m_last_midi_message;

    HookList m_note_callbacks { *this };
    HookList m_cc_callbacks { *this };
    HookList m_pitch_bend_callbacks { *this };

    void fire_midi_callbacks(const Core::InputValue::MIDIMessage& midi);
    [[nodiscard]] bool matches_filters(const Core::InputValue& value) const;
};

} // namespace MayaFlux::Nodes::Input
