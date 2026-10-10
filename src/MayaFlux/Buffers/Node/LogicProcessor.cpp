#include "LogicProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Kinesis/Discrete/Word.hpp"

namespace MayaFlux::Buffers {

LogicProcessor::LogicProcessor(
    const std::shared_ptr<Nodes::Generator::Logic>& logic,
    bool reset_between_buffers)
    : m_logic(logic)
    , m_reset_between_buffers(reset_between_buffers)
    , m_use_internal(false)
    , m_modulation_type(ModulationType::REPLACE)
    , m_has_generated_data(false)
    , m_high_value(1.0)
    , m_low_value(0.0)
    , m_last_held_value(0.0)
    , m_last_logic_value(0.0)
{
}

bool LogicProcessor::generate(size_t num_samples, const std::vector<double>& input_data)
{
    if (!m_logic || input_data.empty()) {
        return false;
    }

    if (m_pending_logic) {
        m_logic = m_pending_logic;
        m_pending_logic.reset();
        m_use_internal = true;
    }

    m_logic_data.resize(num_samples, 0);

    if (m_reset_between_buffers) {
        m_logic->reset();
        for (const auto& node : m_plane_nodes) {
            node->reset();
        }
    }

    const bool guarded = m_logic->m_state.load() != Nodes::NodeState::INACTIVE;
    if (guarded) {
        m_logic->save_state();
    }

    const bool parallel = m_logic->get_mode() == Nodes::Generator::LogicMode::MULTI_INPUT;

    if (m_generations > 0 && parallel) {
        generate_spatial(num_samples, input_data);
    } else if (m_plane_bits > 0) {
        generate_planes(num_samples, input_data);
    } else if (!m_keys.empty() && parallel) {
        generate_keyed(num_samples, input_data);
    } else {
        generate_temporal(num_samples, input_data);
    }

    if (guarded) {
        m_logic->restore_state();
    }

    m_has_generated_data = true;
    return true;
}

void LogicProcessor::generate_temporal(size_t num_samples, const std::vector<double>& input_data)
{
    const size_t available = input_data.size();
    for (size_t i = 0; i < num_samples; ++i) {
        m_logic_data[i] = m_logic->process_sample(i < available ? input_data[i] : 0.0);
    }
}

void LogicProcessor::lock_keys()
{
    m_live_keys.clear();
    for (const auto& key : m_keys) {
        m_live_keys.push_back(key.lock());
    }
}

void LogicProcessor::generate_keyed(size_t num_samples, const std::vector<double>& input_data)
{
    lock_keys();

    const size_t available = input_data.size();
    m_slots.assign(m_live_keys.size() + 1, 0.0);

    for (size_t i = 0; i < num_samples; ++i) {
        for (size_t k = 0; k < m_live_keys.size(); ++k) {
            const auto& key = m_live_keys[k];
            m_slots[k] = (key && i < key->get_data().size()) ? key->get_data()[i] : 0.0;
        }
        m_slots.back() = i < available ? input_data[i] : 0.0;
        m_logic_data[i] = m_logic->process_multi_input(m_slots);
    }
}

void LogicProcessor::generate_planes(size_t num_samples, const std::vector<double>& input_data)
{
    const uint32_t bits = std::clamp(m_plane_bits, uint32_t { 1 }, uint32_t { 24 });

    if (m_plane_nodes.size() != bits || m_plane_template != m_logic) {
        m_plane_nodes.clear();
        for (uint32_t plane = 0; plane < bits; ++plane) {
            m_plane_nodes.push_back(m_logic->clone());
        }
        m_plane_template = m_logic;
    }

    const bool parallel = m_logic->get_mode() == Nodes::Generator::LogicMode::MULTI_INPUT;

    lock_keys();
    const size_t key_count = parallel ? m_live_keys.size() : 0;
    m_key_words.assign(key_count, 0);
    m_slots.assign(key_count + 1, 0.0);

    const size_t available = input_data.size();
    const uint32_t silence = Kinesis::Discrete::to_word(0.0, bits);

    for (size_t i = 0; i < num_samples; ++i) {
        const uint32_t own = Kinesis::Discrete::to_word(i < available ? input_data[i] : 0.0, bits);
        for (size_t k = 0; k < key_count; ++k) {
            const auto& key = m_live_keys[k];
            m_key_words[k] = (key && i < key->get_data().size())
                ? Kinesis::Discrete::to_word(key->get_data()[i], bits)
                : silence;
        }

        uint32_t word = 0;
        for (uint32_t plane = 0; plane < bits; ++plane) {
            const auto bit_of = [plane](uint32_t w) { return static_cast<double>((w >> plane) & 1U); };

            double out = 0.0;
            if (parallel) {
                for (size_t k = 0; k < key_count; ++k) {
                    m_slots[k] = bit_of(m_key_words[k]);
                }
                m_slots.back() = bit_of(own);
                out = m_plane_nodes[plane]->process_multi_input(m_slots);
            } else {
                out = m_plane_nodes[plane]->process_sample(bit_of(own));
            }
            word |= (out > 0.5 ? 1U : 0U) << plane;
        }

        m_logic_data[i] = Kinesis::Discrete::from_word(word, bits);
    }
}

void LogicProcessor::generate_spatial(size_t num_samples, const std::vector<double>& input_data)
{
    if (num_samples == 0) {
        return;
    }

    const size_t available = input_data.size();
    const size_t width = 2 * m_radius + 1;
    const auto count = static_cast<std::ptrdiff_t>(num_samples);

    m_cells.assign(num_samples, 0);
    m_next_cells.assign(num_samples, 0);
    m_slots.assign(width, 0.0);

    for (size_t i = 0; i < num_samples; ++i) {
        uint8_t cell = (i < available && input_data[i] > 0.0) ? 1 : 0;
        if (m_memory && i < m_previous_cells.size()) {
            cell ^= m_previous_cells[i];
        }
        m_cells[i] = cell;
    }

    for (size_t generation = 0; generation < m_generations; ++generation) {
        for (std::ptrdiff_t i = 0; i < count; ++i) {
            for (size_t k = 0; k < width; ++k) {
                std::ptrdiff_t at = i + static_cast<std::ptrdiff_t>(m_radius) - static_cast<std::ptrdiff_t>(k);
                double cell = 0.0;
                if (at >= 0 && at < count) {
                    cell = m_cells[static_cast<size_t>(at)];
                } else if (m_wrap) {
                    at = ((at % count) + count) % count;
                    cell = m_cells[static_cast<size_t>(at)];
                }
                m_slots[k] = cell;
            }
            m_next_cells[static_cast<size_t>(i)] = m_logic->process_multi_input(m_slots) > 0.5 ? 1 : 0;
        }
        m_cells.swap(m_next_cells);
    }

    for (size_t i = 0; i < num_samples; ++i) {
        m_logic_data[i] = m_cells[i];
    }
    if (m_memory) {
        m_previous_cells = m_cells;
    }
}

void LogicProcessor::add_key(const std::shared_ptr<AudioBuffer>& buffer)
{
    if (buffer) {
        m_keys.push_back(buffer);
    }
}

void LogicProcessor::clear_keys()
{
    m_keys.clear();
}

void LogicProcessor::set_bit_planes(uint32_t bits)
{
    m_plane_bits = bits == 0 ? 0 : std::clamp(bits, uint32_t { 1 }, uint32_t { 24 });
    m_plane_nodes.clear();
    m_plane_template.reset();
}

void LogicProcessor::set_neighbourhood(size_t radius, size_t generations, bool wrap, bool memory)
{
    m_radius = radius;
    m_generations = generations;
    m_wrap = wrap;
    m_memory = memory;
    m_previous_cells.clear();
}

void LogicProcessor::clear_neighbourhood()
{
    m_generations = 0;
    m_previous_cells.clear();
}

bool LogicProcessor::apply(const std::shared_ptr<Buffer>& buffer, ModulationFunction modulation_func)
{
    if (!buffer || !m_has_generated_data) {
        return false;
    }

    auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();
    size_t min_size = std::min(m_logic_data.size(), buffer_data.size());

    if (!modulation_func) {
        switch (m_modulation_type) {
        case ModulationType::REPLACE:
            modulation_func = [](double logic_val, double /*buffer_val*/) {
                return logic_val;
            };
            break;

        case ModulationType::MULTIPLY:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val * buffer_val;
            };
            break;

        case ModulationType::ADD:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val + buffer_val;
            };
            break;

        case ModulationType::INVERT_ON_TRUE:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val > 0.5 ? -buffer_val : buffer_val;
            };
            break;

        case ModulationType::HOLD_ON_FALSE:
            if (min_size > 0) {
                m_last_held_value = buffer_data[0];
            }

            modulation_func = [this](double logic_val, double buffer_val) mutable {
                if (logic_val > 0.5) {
                    m_last_held_value = buffer_val;
                    return buffer_val;
                }
                return m_last_held_value;
            };
            break;

        case ModulationType::ZERO_ON_FALSE:
            modulation_func = [](double logic_val, double buffer_val) {
                return logic_val > 0.5 ? buffer_val : 0.0;
            };
            break;

        case ModulationType::CROSSFADE:
            modulation_func = [](double logic_val, double buffer_val) {
                return buffer_val * logic_val;
            };
            break;

        case ModulationType::THRESHOLD_REMAP:
            modulation_func = [this](double logic_val, double /*buffer_val*/) {
                return logic_val > 0.5 ? m_high_value : m_low_value;
            };
            break;

        case ModulationType::SAMPLE_AND_HOLD:
            if (min_size > 0) {
                m_last_held_value = buffer_data[0];
                m_last_logic_value = m_logic_data[0];
            }

            modulation_func = [this, first_sample = true](double logic_val, double buffer_val) mutable {
                if (first_sample) {
                    first_sample = false;
                    return buffer_val;
                }

                bool logic_changed = std::abs(logic_val - m_last_logic_value) > 0.01;
                m_last_logic_value = logic_val;

                if (logic_changed) {
                    m_last_held_value = buffer_val;
                }
                return m_last_held_value;
            };
            break;

        case ModulationType::CUSTOM:
            modulation_func = m_modulation_function;
            break;

        default:
            modulation_func = [](double logic_val, double /*buffer_val*/) {
                return logic_val;
            };
        }
    }

    for (size_t i = 0; i < min_size; ++i) {
        buffer_data[i] = modulation_func(m_logic_data[i], buffer_data[i]);
    }

    return true;
}

void LogicProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_logic || !buffer) {
        return;
    }

    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer || audio_buffer->get_data().empty()) {
        return;
    }

    generate(audio_buffer->get_num_samples(), audio_buffer->get_data());
    apply(buffer);
}

void LogicProcessor::on_attach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_logic) {
        m_logic->reset();
    }

    for (const auto& node : m_plane_nodes) {
        node->reset();
    }
    m_previous_cells.clear();

    m_last_held_value = 0.0;
    m_last_logic_value = 0.0;
}

std::shared_ptr<LogicProcessor> LogicProcessor::clone() const
{
    if (!m_logic) {
        return std::make_shared<LogicProcessor>(std::shared_ptr<Nodes::Generator::Logic> {}, m_reset_between_buffers);
    }

    auto copy = std::make_shared<LogicProcessor>(m_logic->clone(), m_reset_between_buffers);
    copy->m_use_internal = true;
    copy->m_modulation_type = m_modulation_type;
    copy->m_modulation_function = m_modulation_function;
    copy->m_high_value = m_high_value;
    copy->m_low_value = m_low_value;
    copy->m_keys = m_keys;
    copy->m_plane_bits = m_plane_bits;
    copy->m_radius = m_radius;
    copy->m_generations = m_generations;
    copy->m_wrap = m_wrap;
    copy->m_memory = m_memory;
    return copy;
}

void LogicProcessor::set_modulation_function(ModulationFunction func)
{
    m_modulation_function = std::move(func);
    m_modulation_type = ModulationType::CUSTOM;
}

} // namespace MayaFlux::Buffers
