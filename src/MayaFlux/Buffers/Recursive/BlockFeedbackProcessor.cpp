#include "BlockFeedbackProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"

namespace MayaFlux::Buffers {

BlockFeedbackProcessor::BlockFeedbackProcessor(Transform transform, size_t lag_blocks, std::vector<double> coefficients)
    : m_transform(std::move(transform))
    , m_lag_blocks(std::max<size_t>(lag_blocks, 1))
    , m_coefficients(std::move(coefficients))
{
}

void BlockFeedbackProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto [found, inserted] = m_states.try_emplace(buffer.get());
    if (inserted) {
        found->second.coefs = m_coefficients;
    }
}

void BlockFeedbackProcessor::on_detach(const std::shared_ptr<Buffer>& buffer)
{
    m_states.erase(buffer.get());
}

bool BlockFeedbackProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

double BlockFeedbackProcessor::feature(const std::shared_ptr<Buffer>& buffer) const
{
    const auto found = m_states.find(buffer.get());
    return found == m_states.end() ? 0.0 : found->second.feature;
}

std::span<double> BlockFeedbackProcessor::coefficients(const std::shared_ptr<Buffer>& buffer)
{
    const auto found = m_states.find(buffer.get());
    if (found == m_states.end()) {
        return {};
    }
    return found->second.coefs;
}

std::shared_ptr<BlockFeedbackProcessor> BlockFeedbackProcessor::clone() const
{
    auto copy = std::make_shared<BlockFeedbackProcessor>(m_transform, m_lag_blocks, m_coefficients);
    copy->m_retain_input = m_retain_input;
    copy->m_observer = m_observer;
    return copy;
}

void BlockFeedbackProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio || !m_transform) {
        return;
    }

    auto& data = audio->get_data();
    const size_t samples = data.size();
    if (samples == 0) {
        return;
    }

    auto [found, inserted] = m_states.try_emplace(buffer.get());
    State& state = found->second;
    if (inserted) {
        state.coefs = m_coefficients;
    }

    if (state.blocks.size() != m_lag_blocks || state.blocks.front().size() != samples) {
        state.blocks.assign(m_lag_blocks, std::vector<double>(samples, 0.0));
        state.kept.assign(samples, 0.0);
        state.next = 0;
    }

    const std::vector<double>& previous = state.blocks.at(state.next);

    if (m_retain_input) {
        std::ranges::copy(data, state.kept.begin());
    }

    state.feature = m_transform(std::span<double>(data), std::span<const double>(previous), std::span<double>(state.coefs));

    state.blocks.at(state.next) = m_retain_input ? state.kept : data;
    state.next = (state.next + 1) % m_lag_blocks;

    if (m_observer) {
        m_observer(state.feature);
    }
}

} // namespace MayaFlux::Buffers
