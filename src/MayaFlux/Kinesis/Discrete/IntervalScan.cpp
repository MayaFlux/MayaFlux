#include "IntervalScan.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

    uint32_t clamped_sub(uint32_t a, uint32_t b) noexcept
    {
        return a > b ? a - b : 0U;
    }

}

IntervalScanner::IntervalScanner(const IntervalScanConfig& config)
    : m_config(config)
{
    const auto fail = [](std::string_view message) {
        error<std::invalid_argument>(Journal::Component::Kinesis, Journal::Context::Runtime,
            std::source_location::current(), "{}", message);
    };
    if (!std::isfinite(config.enter) || !std::isfinite(config.leave) || !std::isfinite(config.gate_threshold)) {
        fail("Interval scan thresholds must be finite");
    }
    if (config.hop == 0) {
        fail("Interval scan hop must be greater than 0");
    }
    if (config.max_run != 0 && config.max_run <= static_cast<uint64_t>(config.hop) + 2ULL * config.padding) {
        fail("Interval scan run cap must exceed one hop plus twice the padding");
    }
    m_run_limit = config.max_run == 0
        ? std::numeric_limits<uint32_t>::max()
        : config.max_run - config.hop - 2 * config.padding;
}

void IntervalScanner::reset() noexcept
{
    m_seen = 0;
    m_triggered = false;
    m_start = 0;
    m_has_start = false;
    m_pend_valid = false;
    m_pend_start = 0;
    m_pend_end = 0;
    m_best_end = 0;
    m_best_dur = 0;
    reset_gap_state();
}

void IntervalScanner::reset_gap_state() noexcept
{
    m_prev_end = 0;
    m_next_start = 0;
    m_temp_end = 0;
    m_has_best = false;
}

void IntervalScanner::update_total() noexcept
{
    m_total = m_config.length != 0
        ? m_config.length
        : static_cast<uint32_t>(m_config.first_sample + m_seen * m_config.hop);
}

void IntervalScanner::close_run(uint32_t start, uint32_t end, std::vector<Interval>& out)
{
    if (m_config.emit_on_close) {
        out.emplace_back(clamped_sub(start, m_config.padding), std::min(m_total, end + m_config.padding));
        return;
    }
    uint32_t padded_start { 0 };
    if (m_pend_valid) {
        const int32_t gap = static_cast<int32_t>(start) - static_cast<int32_t>(m_pend_end);
        if (gap < static_cast<int32_t>(2U * m_config.padding)) {
            const int32_t half_gap = gap >= 0 ? gap / 2 : -((-gap + 1) / 2);
            const auto padded_end = static_cast<uint32_t>(static_cast<int32_t>(m_pend_end) + half_gap);
            padded_start = half_gap >= 0
                ? clamped_sub(start, static_cast<uint32_t>(half_gap))
                : start + static_cast<uint32_t>(-half_gap);
            out.emplace_back(m_pend_start, padded_end);
        } else {
            const uint32_t padded_end = std::min(m_total, m_pend_end + m_config.padding);
            padded_start = clamped_sub(start, m_config.padding);
            out.emplace_back(m_pend_start, padded_end);
        }
    } else {
        padded_start = clamped_sub(start, m_config.padding);
    }
    m_pend_valid = true;
    m_pend_start = padded_start;
    m_pend_end = end;
}

std::vector<IntervalScanner::Interval> IntervalScanner::push(std::span<const double> scores, std::span<const double> gate)
{
    if (!gate.empty() && gate.size() != scores.size()) {
        error<std::invalid_argument>(Journal::Component::Kinesis, Journal::Context::Runtime,
            std::source_location::current(), "Interval scan gate has {} scores for {} scores", gate.size(), scores.size());
    }

    const auto first = static_cast<uint32_t>(m_seen);
    m_seen += scores.size();
    update_total();

    std::vector<Interval> out;
    const bool gated = !gate.empty();

    for (size_t i = 0; i < scores.size(); ++i) {
        const double score = static_cast<float>(scores[i]);
        const uint32_t cur = m_config.first_sample + (first + static_cast<uint32_t>(i)) * m_config.hop;
        const bool above = score >= m_config.enter;

        if (gated && static_cast<double>(static_cast<float>(gate[i])) < m_config.gate_threshold) {
            if (m_triggered) {
                const uint32_t end = m_temp_end != 0 ? m_temp_end : cur;
                if (end > m_start && end - m_start > m_config.min_run) {
                    close_run(m_start, end, out);
                }
            }
            m_has_start = false;
            m_triggered = false;
            reset_gap_state();
            continue;
        }

        if (above && m_temp_end != 0) {
            const uint32_t silence = cur - m_temp_end;
            if (silence > m_config.gap_at_limit) {
                if (!m_has_best || silence > m_best_dur) {
                    m_best_end = m_temp_end;
                    m_best_dur = silence;
                    m_has_best = true;
                }
            }
            m_temp_end = 0;
            if (m_next_start < m_prev_end) {
                m_next_start = cur;
            }
        }

        if (above && !m_triggered) {
            m_triggered = true;
            m_start = cur;
            m_has_start = true;
            continue;
        }

        if (m_triggered && cur > m_start && cur - m_start > m_run_limit) {
            if (m_config.use_longest_gap && m_has_best) {
                const uint32_t cut = m_best_end;
                const uint32_t dur = m_best_dur;
                close_run(m_start, cut, out);
                m_has_start = false;
                const uint32_t resume = cut + dur;
                if (resume < cut + cur) {
                    m_start = resume;
                    m_has_start = true;
                } else {
                    m_triggered = false;
                }
                reset_gap_state();
            } else if (m_prev_end != 0) {
                const uint32_t cut = m_prev_end;
                close_run(m_start, cut, out);
                m_has_start = false;
                if (m_next_start < cut) {
                    m_triggered = false;
                } else {
                    m_start = m_next_start;
                    m_has_start = true;
                }
                reset_gap_state();
            } else {
                close_run(m_start, cur, out);
                m_has_start = false;
                reset_gap_state();
                m_triggered = false;
                continue;
            }
        }

        if (score < m_config.leave && m_triggered) {
            if (m_temp_end == 0) {
                m_temp_end = cur;
            }
            const uint32_t quiet = cur - m_temp_end;

            if (!m_config.use_longest_gap && quiet > m_config.gap_at_limit) {
                m_prev_end = m_temp_end;
            }
            if (quiet < m_config.min_gap) {
                continue;
            }

            const uint32_t end = m_temp_end;
            if (end > m_start && end - m_start > m_config.min_run) {
                close_run(m_start, end, out);
            }
            m_has_start = false;
            m_triggered = false;
            reset_gap_state();
        }
    }

    return out;
}

std::vector<IntervalScanner::Interval> IntervalScanner::flush()
{
    update_total();
    std::vector<Interval> out;

    if (m_has_start && m_total > m_start && m_total - m_start > m_config.min_run) {
        close_run(m_start, m_total, out);
    }
    if (m_pend_valid) {
        out.emplace_back(m_pend_start, std::min(m_total, m_pend_end + m_config.padding));
    }

    reset();
    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
