#include "RelationOperators.hpp"

#include "MayaFlux/Nodes/Network/RelationNetwork.hpp"

namespace MayaFlux::Nodes::Network {

void StepOperator::process(float frames)
{
    if (m_body) {
        m_body(*this, slots(), static_cast<uint32_t>(frames));
    }
}

void StepOperator::set_parameter(std::string_view param, double value)
{
    m_parameters.insert_or_assign(std::string(param), value);
}

std::optional<double> StepOperator::query_state(std::string_view query) const
{
    auto it = m_parameters.find(query);
    if (it == m_parameters.end()) {
        return std::nullopt;
    }
    return it->second;
}

double StepOperator::parameter(std::string_view name) const
{
    auto it = m_parameters.find(name);
    return it == m_parameters.end() ? 0.0 : it->second;
}

void AdvanceOperator::process(float frames)
{
    const auto num_frames = static_cast<uint32_t>(frames);

    for (auto& slot : slots()) {
        const bool active = slot.active;
        auto block = slot.prepare_block(num_frames);

        if (slot.node) {
            m_host.advance_source(slot.node, m_scratch, m_scratch_pos, num_frames);
            for (uint32_t i = 0; i < num_frames; ++i) {
                block[i] = active ? m_scratch[i] : 0.0;
            }
            if (num_frames > 0) {
                slot.level = m_scratch[num_frames - 1];
            }
        } else {
            const double level = slot.level;
            for (uint32_t i = 0; i < num_frames; ++i) {
                block[i] = active ? level : 0.0;
            }
        }
    }
}

DeriveOperator::DeriveOperator(size_t target, std::vector<size_t> inputs, Function function)
    : m_target(target)
    , m_inputs(std::move(inputs))
    , m_values(m_inputs.size(), 0.0)
    , m_function(std::move(function))
{
}

void DeriveOperator::process(float frames)
{
    if (!m_function) {
        return;
    }

    const auto num_frames = static_cast<uint32_t>(frames);
    auto all = slots();
    auto& target = all[m_target];
    auto out = target.prepare_block(num_frames);

    for (uint32_t frame = 0; frame < num_frames; ++frame) {
        for (size_t i = 0; i < m_inputs.size(); ++i) {
            const size_t input_index = m_inputs[i];
            if (input_index == m_target) {
                m_values[i] = frame == 0 ? target.level : out[frame - 1];
            } else {
                m_values[i] = all[input_index].block.size() > frame
                    ? all[input_index].block[frame]
                    : all[input_index].level;
            }
        }
        out[frame] = m_function(m_values);
    }

    if (num_frames > 0) {
        target.level = out[num_frames - 1];
    }
}

CombineOperator::CombineOperator(size_t slot_count, size_t output_count)
    : m_weights(slot_count, std::max<size_t>(1, output_count))
    , m_outputs(std::max<size_t>(1, output_count))
    , m_values(slot_count, 0.0)
{
    for (size_t row = 0; row < slot_count; ++row) {
        m_weights(row, 0) = 1.0;
    }
}

void CombineOperator::process(float frames)
{
    const auto num_frames = static_cast<uint32_t>(frames);
    auto all = slots();

    const size_t count = std::min(all.size(), m_weights.rows());
    if (m_values.size() < count) {
        m_values.resize(count, 0.0);
    }
    const std::span<const double> values(m_values.data(), count);

    for (auto& output : m_outputs) {
        output.m_block.assign(num_frames, 0.0);
    }

    for (uint32_t frame = 0; frame < num_frames; ++frame) {
        for (size_t m = 0; m < count; ++m) {
            m_values[m] = all[m].block.size() > frame ? all[m].block[frame] : all[m].level;
        }

        for (size_t o = 0; o < m_outputs.size(); ++o) {
            auto& output = m_outputs[o];
            if (output.has_combine()) {
                output.m_block[frame] = output.m_function(values);
                continue;
            }
            double sum = 0.0;
            for (size_t m = 0; m < count; ++m) {
                sum += m_values[m] * m_weights.get(m, o);
            }
            output.m_block[frame] = sum;
        }
    }
}

std::span<const double> CombineOperator::get_output_block(size_t index) const
{
    return m_outputs[index].block();
}

} // namespace MayaFlux::Nodes::Network
