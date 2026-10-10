#include "FeedbackBuffer.hpp"

#include "MayaFlux/Buffers/BufferSpec.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

//-----------------------------------------------------------------------------
// FeedbackBuffer
//-----------------------------------------------------------------------------

FeedbackBuffer::FeedbackBuffer(uint32_t channel_id, uint32_t num_samples,
    float feedback, uint32_t feed_samples)
    : AudioBuffer(channel_id, num_samples)
    , m_feedback_amount(feedback)
    , m_feed_samples(std::max<uint32_t>(feed_samples, 1))
    , m_history(m_feed_samples)
{
    m_default_processor = create_default_processor();
}

void FeedbackBuffer::process_default()
{
    m_default_processor->process(shared_from_this());
}

void FeedbackBuffer::set_feedback(float amount)
{
    m_feedback_amount = amount;
    if (auto proc = std::dynamic_pointer_cast<FeedbackProcessor>(m_default_processor)) {
        proc->set_feedback(amount);
    }
}

void FeedbackBuffer::set_feed_samples(uint32_t samples)
{
    m_feed_samples = std::max<uint32_t>(samples, 1);
    m_history.resize(m_feed_samples);
    if (auto proc = std::dynamic_pointer_cast<FeedbackProcessor>(m_default_processor)) {
        proc->set_feed_samples(m_feed_samples);
    }
}

std::shared_ptr<BufferProcessor> FeedbackBuffer::create_default_processor()
{
    return std::make_shared<FeedbackProcessor>(m_feedback_amount, m_feed_samples);
}

std::shared_ptr<AudioBuffer> FeedbackBuffer::clone_to(uint32_t channel)
{
    auto clone = std::make_shared<FeedbackBuffer>(channel, get_num_samples(), m_feedback_amount, m_feed_samples);
    clone->get_data() = get_data();

    if (auto proc = std::dynamic_pointer_cast<FeedbackProcessor>(m_default_processor)) {
        clone->set_default_processor(proc->clone());
    }

    clone->set_processing_chain(get_processing_chain(), true);
    return clone;
}

//-----------------------------------------------------------------------------
// FeedbackProcessor
//-----------------------------------------------------------------------------

FeedbackProcessor::FeedbackProcessor(float feedback, uint32_t feed_samples)
    : m_feedback_amount(feedback)
    , m_feed_samples(std::max<uint32_t>(feed_samples, 1))
    , m_default_combine(true)
{
    m_combine = [this](double x, std::span<const double> taps, std::span<double>) {
        return x + static_cast<double>(m_feedback_amount) * taps[0];
    };

    Tap tap;
    tap.samples = static_cast<double>(m_feed_samples);
    tap.max_samples = tap.samples;
    m_taps.push_back(std::move(tap));
    recompute_capacity();
}

FeedbackProcessor::FeedbackProcessor(Combine combine, const std::vector<double>& lags, std::vector<double> coefficients)
    : m_combine(std::move(combine))
    , m_coefficients(std::move(coefficients))
{
    set_lags(lags);
}

FeedbackProcessor::FeedbackProcessor(CombineStep combine, const std::vector<double>& lags, std::vector<double> coefficients)
    : m_step(std::move(combine))
    , m_coefficients(std::move(coefficients))
{
    set_lags(lags);
}

void FeedbackProcessor::set_feed_samples(uint32_t samples)
{
    m_feed_samples = std::max<uint32_t>(samples, 1);
    if (m_taps.empty()) {
        m_taps.emplace_back();
    }
    m_taps.front().samples = static_cast<double>(m_feed_samples);
    m_taps.front().map.reset();
    m_taps.front().max_samples = m_taps.front().samples;
    recompute_capacity();
}

void FeedbackProcessor::set_combine(Combine combine)
{
    m_combine = std::move(combine);
    m_step = nullptr;
    m_default_combine = false;
}

void FeedbackProcessor::set_combine(CombineStep combine)
{
    m_step = std::move(combine);
    m_combine = nullptr;
    m_default_combine = false;
}

void FeedbackProcessor::set_lags(const std::vector<double>& lags)
{
    m_taps.clear();
    m_taps.reserve(lags.size());
    for (const double lag : lags) {
        Tap tap;
        tap.samples = std::max(lag, 1.0);
        tap.max_samples = tap.samples;
        m_taps.push_back(std::move(tap));
    }

    if (!m_taps.empty()) {
        m_feed_samples = static_cast<uint32_t>(std::max(std::round(m_taps.front().samples), 1.0));
    }
    recompute_capacity();
}

void FeedbackProcessor::set_lag(size_t tap, Kinesis::TimeMap seconds, double max_seconds)
{
    if (tap >= m_taps.size()) {
        error<std::out_of_range>(Journal::Component::Buffers, Journal::Context::Configuration, std::source_location::current(),
            "FeedbackProcessor::set_lag: tap out of range (tap={}, tap_count={})", tap, m_taps.size());
    }

    m_taps[tap].map = std::make_shared<const Kinesis::TimeMap>(std::move(seconds));
    m_taps[tap].max_samples = std::max(1.0, max_seconds * static_cast<double>(s_registered_sample_rate));
    recompute_capacity();
}

void FeedbackProcessor::set_coefficients(std::vector<double> coefficients)
{
    m_coefficients = std::move(coefficients);
    for (auto& [buffer, state] : m_states) {
        state.coefs = m_coefficients;
    }
}

std::span<double> FeedbackProcessor::coefficients(const std::shared_ptr<Buffer>& buffer)
{
    const auto found = m_states.find(buffer.get());
    if (found == m_states.end()) {
        return {};
    }
    return found->second.coefs;
}

std::shared_ptr<FeedbackProcessor> FeedbackProcessor::clone() const
{
    auto copy = std::make_shared<FeedbackProcessor>(m_feedback_amount, m_feed_samples);

    copy->m_taps = m_taps;
    copy->m_coefficients = m_coefficients;
    copy->m_capacity = m_capacity;

    if (!m_default_combine) {
        copy->m_default_combine = false;
        copy->m_combine = m_combine;
        copy->m_step = m_step;
    }

    return copy;
}

void FeedbackProcessor::recompute_capacity()
{
    double needed = 1.0;
    for (const auto& tap : m_taps) {
        needed = std::max(needed, tap.max_samples);
    }
    m_capacity = static_cast<size_t>(std::ceil(needed));
}

FeedbackProcessor::State& FeedbackProcessor::state_for(const std::shared_ptr<Buffer>& buffer)
{
    auto [found, inserted] = m_states.try_emplace(buffer.get());
    State& state = found->second;

    if (inserted) {
        state.coefs = m_coefficients;
        state.values.assign(m_taps.size(), 0.0);
        state.lag_prev.assign(m_taps.size(), 1.0);
        state.lag_now.assign(m_taps.size(), 1.0);

        if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
            state.ring = &feedback_buffer->get_history_buffer();
        } else {
            state.own.resize(m_capacity);
            state.ring = &state.own;
        }
    }

    return state;
}

void FeedbackProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    state_for(buffer);
}

void FeedbackProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    m_states.erase(buffer.get());
}

bool FeedbackProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

double FeedbackProcessor::read(const Memory::HistoryBuffer<double>& ring, double lag)
{
    const auto whole = static_cast<size_t>(lag);
    const double fraction = lag - static_cast<double>(whole);
    const double nearer = ring[whole - 1];

    if (fraction <= 0.0) {
        return nearer;
    }
    return nearer + (ring[whole] - nearer) * fraction;
}

void FeedbackProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        return;
    }

    const bool use_step = static_cast<bool>(m_step);
    if (!use_step && !m_combine) {
        return;
    }

    auto& data = audio->get_data();
    const size_t samples = data.size();
    const size_t taps = m_taps.size();
    if (samples == 0 || taps == 0) {
        return;
    }

    State& state = state_for(buffer);
    auto& ring = *state.ring;

    if (ring.capacity() < m_capacity) {
        ring.resize(m_capacity);
    }

    if (state.values.size() != taps) {
        state.values.assign(taps, 0.0);
        state.lag_prev.assign(taps, 1.0);
        state.lag_now.assign(taps, 1.0);
        state.primed = false;
    }

    const auto rate = static_cast<double>(s_registered_sample_rate);
    const auto seconds = static_cast<double>(state.elapsed) / rate;
    const auto limit = static_cast<double>(ring.capacity());

    for (size_t t = 0; t < taps; ++t) {
        const double raw = m_taps[t].map ? (*m_taps[t].map)(seconds) * rate : m_taps[t].samples;
        state.lag_now[t] = std::clamp(raw, 1.0, std::max(limit, 1.0));
    }
    if (!state.primed) {
        state.lag_prev = state.lag_now;
        state.primed = true;
    }

    const double inverse = 1.0 / static_cast<double>(samples);

    for (size_t i = 0; i < samples; ++i) {
        const double ramp = static_cast<double>(i + 1) * inverse;
        for (size_t t = 0; t < taps; ++t) {
            const double lag = state.lag_prev[t] + (state.lag_now[t] - state.lag_prev[t]) * ramp;
            state.values[t] = read(ring, lag);
        }

        const std::span<const double> tap_values(state.values);
        const std::span<double> coefs(state.coefs);

        double out {};
        double carry {};
        if (use_step) {
            const auto [step_out, step_carry] = m_step(data[i], tap_values, coefs);
            out = step_out;
            carry = step_carry;
        } else {
            out = m_combine(data[i], tap_values, coefs);
            carry = out;
        }

        data[i] = out;
        ring.push(carry);
    }

    state.lag_prev = state.lag_now;
    state.elapsed += samples;
}

} // namespace MayaFlux::Buffers
