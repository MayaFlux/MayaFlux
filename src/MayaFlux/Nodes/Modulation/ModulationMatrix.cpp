#include "ModulationMatrix.hpp"

namespace MayaFlux::Nodes::Modulation {

void ModMatrix::connect_modulator(std::shared_ptr<Node> modulator, ModulationTarget target, std::string targetID, float amount)
{
    if (!modulator)
        return;

    target_key key { target, targetID };

    auto& connections = m_connections.at(key);

    for (auto& conn : connections) {
        if (conn.modulator.get() == modulator.get()) {
            conn.amount = amount;
            return;
        }
    }

    connections.push_back({ modulator, amount, 0.0f });
}

void ModMatrix::disconnect_modulator(std::shared_ptr<Node> modulator, ModulationTarget target, std::string targetID)
{
    if (!modulator)
        return;

    target_key key { target, targetID };

    auto it = m_connections.find(key);

    if (it != m_connections.end()) {
        auto& connections = it->second;

        connections.erase(
            std::remove_if(
                connections.begin(), connections.end(), [&modulator](const mod_connection& conn) { return conn.modulator.get() == modulator.get(); }),
            connections.end());

        if (connections.empty()) {
            m_connections.erase(it);
            m_mod_values.erase(key);
        }
    }
}

// void ModMatrix::remove_modulator(std::shared_ptr<Node> modulator);

// void ModMatrix::clear_target(ModulationTarget target, std::string targetID);

// void ModMatrix::clear_all();

// floatModMatrix:: get_modulation_value(ModulationTarget target, std::string targetID);

// void ModMatrix::process_mods();

}
