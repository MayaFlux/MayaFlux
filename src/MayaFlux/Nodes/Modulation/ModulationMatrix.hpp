#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Modulation {

enum class ModulationTarget {
    FREQUENCY,
    AMPLITUDE,
    OFFSET,
    FILTER_CUTOFF,
    FILTER_RESONANCE,
    FILTER_DRIVE,
    PAN,
};

class ModMatrix {

    void connect_modulator(std::shared_ptr<Node> modulator, ModulationTarget target, std::string targetID, float amount = 1.f);

    void disconnect_modulator(std::shared_ptr<Node> modulator, ModulationTarget target, std::string targetID);

    void remove_modulator(std::shared_ptr<Node> modulator);

    void clear_target(ModulationTarget target, std::string targetID);

    void clear_all();

    float get_modulation_value(ModulationTarget target, std::string targetID);

    void process_mods();

private:
    using target_key = std::pair<ModulationTarget, std::string>;

    struct target_key_hash {
        size_t operator()(const target_key& key) const
        {
            return std::hash<int>()(static_cast<int>(key.first)) ^ std::hash<std::string>()(key.second);
        }
    };

    struct mod_connection {
        std::shared_ptr<Node> modulator;
        float amount;
        float last_value;
    };

    std::unordered_map<target_key, std::vector<mod_connection>, target_key_hash> m_connections;
    std::unordered_map<target_key, float, std::vector<mod_connection>> m_mod_values;
};

}
