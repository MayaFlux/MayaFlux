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

    // Chaining methods
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
    explicit MIDINode(MIDIConfig config = {});

    void save_state() override { }
    void restore_state() override { }

protected:
    double extract_value(const Core::InputValue& value) override;

private:
    MIDIConfig m_config;

    [[nodiscard]] bool matches_filters(const Core::InputValue& value) const;
};

} // namespace MayaFlux::Nodes::Input
