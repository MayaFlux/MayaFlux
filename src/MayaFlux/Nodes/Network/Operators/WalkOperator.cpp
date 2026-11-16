#include "WalkOperator.hpp"

namespace MayaFlux::Nodes::Network {

WalkOperator::WalkOperator(std::shared_ptr<Kinesis::Stochastic::Weights> table, Pick pick,
    uint32_t every_n_blocks, size_t starting_index)
    : m_controls(declare_controls<WalkControls>())
    , m_table(std::move(table))
    , m_pick(std::move(pick))
    , m_current(starting_index)
    , m_blocks_until(every_n_blocks)
{
    m_every_n_blocks.store(every_n_blocks, std::memory_order_relaxed);
}

void WalkOperator::ensure_established()
{
    if (m_established) {
        return;
    }
    auto all = slots();
    if (all.empty()) {
        return;
    }
    const size_t here = m_current.load(std::memory_order_relaxed);
    for (size_t i = 0; i < all.size(); ++i) {
        all[i].set_active(i == here);
    }
    m_established = true;
}

void WalkOperator::move_to(double index)
{
    const auto target = static_cast<size_t>(index);
    if (target != m_current.load(std::memory_order_relaxed)) {
        jump(target);
    }
}

void WalkOperator::read_controls(WalkControls& controls) const
{
    controls.current.observe(static_cast<double>(current()));
    controls.every_n_blocks.observe(static_cast<double>(m_every_n_blocks.load(std::memory_order_relaxed)));
}

void WalkOperator::process(float /*frames*/)
{
    ensure_established();

    refresh_controls();
    if (const auto index = m_controls.current.pending()) {
        move_to(*index);
    }
    if (const auto every = m_controls.every_n_blocks.pending()) {
        apply_every_n_blocks(static_cast<uint32_t>(std::max(0.0, *every)));
    }

    const uint32_t every = m_every_n_blocks.load(std::memory_order_relaxed);
    if (every == 0) {
        return;
    }
    if (m_blocks_until == 0) {
        m_blocks_until = every;
    }
    --m_blocks_until;
    if (m_blocks_until == 0) {
        advance();
    }
}

void WalkOperator::set_every_n_blocks(uint32_t every_n_blocks) noexcept
{
    m_controls.every_n_blocks.settle();
    apply_every_n_blocks(every_n_blocks);
}

void WalkOperator::apply_every_n_blocks(uint32_t every_n_blocks) noexcept
{
    m_every_n_blocks.store(every_n_blocks, std::memory_order_relaxed);
    m_blocks_until = every_n_blocks;
}

size_t WalkOperator::advance()
{
    ensure_established();

    const size_t here = m_current.load(std::memory_order_relaxed);
    auto row = m_table->row(here);

    const size_t next = m_pick ? m_pick(row) : Kinesis::Stochastic::Weights::weighted_pick(row, m_generator);
    if (next >= row.size()) {
        return here;
    }

    jump(next);
    return next;
}

void WalkOperator::jump(size_t index)
{
    ensure_established();

    auto all = slots();
    if (index >= all.size() || index >= m_table->rows()) {
        return;
    }

    const size_t here = m_current.load(std::memory_order_relaxed);
    if (index == here) {
        return;
    }

    all[here].set_active(false);
    all[index].set_active(true);
    m_current.store(index, std::memory_order_relaxed);
}

void WalkOperator::set_parameter(std::string_view param, double value)
{
    settle_named_control(param);
    if (param == "current") {
        move_to(value);
    } else if (param == "every_n_blocks") {
        apply_every_n_blocks(static_cast<uint32_t>(std::max(0.0, value)));
    }
}

std::optional<double> WalkOperator::query_state(std::string_view query) const
{
    if (query == "current") {
        return static_cast<double>(current());
    }
    return std::nullopt;
}

} // namespace MayaFlux::Nodes::Network
