#include "Counter.hpp"

namespace MayaFlux::Nodes::Generator {

Counter::Counter(uint32_t modulo, int32_t step)
    : m_modulo(modulo)
    , m_step(step)
{
    m_amplitude = 1.0;
    m_frequency = 0.F;
}

Counter::Counter(const std::shared_ptr<Node>& reset_trigger, uint32_t modulo, int32_t step)
    : m_modulo(modulo)
    , m_step(step)
    , m_reset_trigger(reset_trigger)
{
    m_amplitude = 1.0;
    m_frequency = 0.F;
}

void Counter::set_modulo(uint32_t modulo) { m_modulo = modulo; }
void Counter::set_step(int32_t step) { m_step = step; }
void Counter::set_reset_trigger(const std::shared_ptr<Node>& trigger) { m_reset_trigger = trigger; }
void Counter::clear_reset_trigger() { m_reset_trigger = nullptr; }
void Counter::reset() { m_count = 0; }

void Counter::on_increment(const TypedHook<GeneratorContext>& callback) { m_increment_callbacks.push_back(callback); }
void Counter::on_wrap(const TypedHook<GeneratorContext>& callback) { m_wrap_callbacks.push_back(callback); }
void Counter::on_count(uint32_t target, const TypedHook<GeneratorContext>& callback) { m_count_callbacks.emplace_back(target, callback); }

bool Counter::remove_hook(const NodeHook& callback)
{
    bool r = safe_remove_callback(m_callbacks, callback);
    auto erase_typed = [&](auto& vec) {
        auto before = vec.size();
        std::erase_if(vec, [&](const auto& cb) {
            return cb.target_type() == callback.target_type();
        });
        return vec.size() < before;
    };
    r |= erase_typed(m_increment_callbacks);
    r |= erase_typed(m_wrap_callbacks);
    std::erase_if(m_count_callbacks, [&](const auto& p) {
        return p.second.target_type() == callback.target_type();
    });
    return r;
}

void Counter::remove_all_hooks()
{
    m_callbacks.clear();
    m_conditional_callbacks.clear();
    m_increment_callbacks.clear();
    m_wrap_callbacks.clear();
    m_count_callbacks.clear();
}

double Counter::process_sample(double /*input*/)
{
    if (m_reset_trigger) {
        atomic_inc_modulator_count(m_reset_trigger->m_modulator_count, 1);
        uint32_t state = m_reset_trigger->m_state.load();

        double trigger_val = 0.0;
        if (state & NodeState::PROCESSED) {
            trigger_val = m_reset_trigger->get_last_output();
        } else {
            trigger_val = m_reset_trigger->process_sample(0.0);
            atomic_add_flag(m_reset_trigger->m_state, NodeState::PROCESSED);
        }

        if (trigger_val != 0.0 && m_last_trigger_value == 0.0) {
            m_count = 0;
        }
        m_last_trigger_value = trigger_val;
    }

    m_wrapped = false;
    if (m_modulo > 0) {
        auto prev = static_cast<int32_t>(m_count);
        int32_t next = (prev + m_step);
        int32_t wrapped = ((next % static_cast<int32_t>(m_modulo)) + static_cast<int32_t>(m_modulo)) % static_cast<int32_t>(m_modulo);
        m_wrapped = (wrapped < prev && m_step > 0) || (wrapped > prev && m_step < 0);
        m_count = static_cast<uint32_t>(wrapped);
    } else {
        m_count += static_cast<uint32_t>(m_step);
    }

    double output = (m_modulo > 1)
        ? static_cast<double>(m_count) / static_cast<double>(m_modulo - 1)
        : static_cast<double>(m_count);

    output *= m_amplitude;
    m_last_output = output;

    if ((!m_state_saved || (m_state_saved && m_fire_events_during_snapshot))
        && !m_networked_node) {
        notify_tick(output);
    }

    if (m_reset_trigger) {
        atomic_dec_modulator_count(m_reset_trigger->m_modulator_count, 1);
        try_reset_processed_state(m_reset_trigger);
    }

    return output;
}

std::vector<double> Counter::process_batch(unsigned int num_samples)
{
    std::vector<double> out(num_samples);
    for (unsigned int i = 0; i < num_samples; ++i) {
        out[i] = process_sample(0.0);
    }
    return out;
}

void Counter::notify_tick(double value)
{
    update_context(value);
    auto& ctx = get_last_context();
    auto& gc = dynamic_cast<GeneratorContext&>(ctx);

    for (auto& cb : m_callbacks) {
        cb(ctx);
    }
    for (auto& [cb, cond] : m_conditional_callbacks) {
        if (cond(ctx)) {
            cb(ctx);
        }
    }
    for (auto& cb : m_increment_callbacks) {
        cb(gc);
    }
    if (m_wrapped) {
        for (auto& cb : m_wrap_callbacks) {
            cb(gc);
        }
    }
    for (auto& [target, cb] : m_count_callbacks) {
        if (m_count == target) {
            cb(gc);
        }
    }
}

void Counter::update_context(double value)
{
    m_context = GeneratorContext(value, 0.F, m_amplitude, static_cast<double>(m_count));
}

NodeContext& Counter::get_last_context()
{
    return m_context;
}

void Counter::save_state()
{
    m_saved_count = m_count;
    m_saved_last_trigger_value = m_last_trigger_value;
    m_saved_last_output = m_last_output;

    if (m_reset_trigger) {
        m_reset_trigger->save_state();
    }

    m_state_saved = true;
}

void Counter::restore_state()
{
    m_count = m_saved_count;
    m_last_trigger_value = m_saved_last_trigger_value;
    m_last_output = m_saved_last_output;

    if (m_reset_trigger) {
        m_reset_trigger->restore_state();
    }

    m_state_saved = false;
}

} // namespace MayaFlux::Nodes::Generatorx::Nodes::Generator
