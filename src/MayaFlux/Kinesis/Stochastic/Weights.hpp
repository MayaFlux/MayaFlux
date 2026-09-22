#pragma once

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Stochastic/Stochastic.hpp"

namespace MayaFlux::Kinesis::Stochastic {

/**
 * @class Weights
 * @brief Rows-by-columns table of doubles with atomic cell access
 *
 * A plain numeric table with no knowledge of what its rows and columns mean.
 * Transition probabilities, source-to-output weights, and neighbour strengths
 * are the same structure read by different code.
 *
 * Cells are read and written with relaxed atomics, so any thread can write a
 * cell while another reads it. Row helpers and row views are not atomic and
 * belong on the thread that consumes the table. Copying is not synchronised.
 * Dimensions change only through assign(), which must not run concurrently
 * with any other access.
 *
 * weighted_pick() turns a row into a single stochastic choice, drawing from
 * a Stochastic instance the caller supplies and owns (one instance per
 * thread, matching Stochastic's own contract).
 *
 * ## Usage
 *
 * ```cpp
 * Weights transitions(5, 5);
 * transitions.normalize_row(0);
 *
 * Stochastic draw(Algorithm::UNIFORM);
 * size_t next = transitions.weighted_pick(0, draw);
 * ```
 */
class Weights {
public:
    static_assert(std::atomic_ref<double>::is_always_lock_free);

    /**
     * @class Cell
     * @brief Proxy for one cell with atomic load and store
     */
    class Cell {
    public:
        Cell& operator=(double value) noexcept
        {
            std::atomic_ref<double>(m_data[m_index]).store(value, std::memory_order_relaxed);
            return *this;
        }

        Cell& operator=(const Cell& other) noexcept
        {
            return *this = static_cast<double>(other);
        }

        [[nodiscard]] operator double() const noexcept
        {
            return std::atomic_ref<double>(m_data[m_index]).load(std::memory_order_relaxed);
        }

    private:
        friend class Weights;

        Cell(std::vector<double>& data, size_t index) noexcept
            : m_data(data)
            , m_index(index)
        {
        }

        std::vector<double>& m_data;
        size_t m_index;
    };

    Weights() = default;

    /**
     * @param rows Row count
     * @param cols Column count
     * @param initial Value of every cell
     */
    Weights(size_t rows, size_t cols, double initial = 0.0)
        : m_rows(rows)
        , m_cols(cols)
        , m_data(rows * cols, initial)
    {
    }

    /**
     * @brief Assign every cell from nested initializer lists
     * @param values One inner list per row. Dimensions must match exactly.
     * @throws std::invalid_argument on dimension mismatch
     */
    Weights& operator=(std::initializer_list<std::initializer_list<double>> values)
    {
        if (values.size() != m_rows) {
            error<std::invalid_argument>(Journal::Component::Kinesis, Journal::Context::Runtime, std::source_location::current(),
                "Weights: row count mismatch. Expected {} rows, received {}", m_rows, values.size());
        }
        size_t row = 0;
        for (const auto& source : values) {
            if (source.size() != m_cols) {
                error<std::invalid_argument>(Journal::Component::Kinesis, Journal::Context::Runtime, std::source_location::current(),
                    "Weights: column count mismatch at row {}. Expected {} cols, received {}", row, m_cols, source.size());
            }
            size_t col = 0;
            for (double value : source) {
                (*this)(row, col++) = value;
            }
            ++row;
        }
        return *this;
    }

    /**
     * @brief Replace dimensions and reset every cell
     */
    void assign(size_t rows, size_t cols, double initial = 0.0)
    {
        m_rows = rows;
        m_cols = cols;
        m_data.assign(rows * cols, initial);
    }

    [[nodiscard]] size_t rows() const noexcept { return m_rows; }
    [[nodiscard]] size_t cols() const noexcept { return m_cols; }

    /**
     * @brief Access one cell
     * @param row Row index, unchecked
     * @param col Column index, unchecked
     */
    [[nodiscard]] Cell operator()(size_t row, size_t col) noexcept
    {
        return { m_data, row * m_cols + col };
    }

    /**
     * @brief Atomic read of one cell
     */
    [[nodiscard]] double get(size_t row, size_t col) const noexcept
    {
        return std::atomic_ref<double>(const_cast<double&>(m_data[row * m_cols + col]))
            .load(std::memory_order_relaxed);
    }

    /**
     * @brief Non-atomic view of one row
     */
    [[nodiscard]] std::span<const double> row(size_t row) const noexcept
    {
        return { m_data.data() + row * m_cols, m_cols };
    }

    void fill(double value) noexcept
    {
        for (double& slot : m_data) {
            std::atomic_ref<double>(slot).store(value, std::memory_order_relaxed);
        }
    }

    void scale_row(size_t row, double factor) noexcept
    {
        for (size_t col = 0; col < m_cols; ++col) {
            Cell cell = (*this)(row, col);
            cell = static_cast<double>(cell) * factor;
        }
    }

    /**
     * @brief Scale a row so its cells sum to one
     * @note A row that sums to zero is left unchanged.
     */
    void normalize_row(size_t row) noexcept
    {
        double sum = 0.0;
        for (size_t col = 0; col < m_cols; ++col) {
            sum += get(row, col);
        }
        if (sum != 0.0) {
            scale_row(row, 1.0 / sum);
        }
    }

    /**
     * @brief Weighted random draw over a row of nonnegative weights
     * @param weights Row to draw from. Negative entries are treated as zero.
     * @param generator Entropy source for the draw
     * @return Index into weights, or weights.size() when every weight is
     *         zero or negative and no draw is possible
     *
     * Does not require a Weights instance or normalized weights; any span
     * of nonnegative values is a valid distribution to draw from.
     */
    [[nodiscard]] static size_t weighted_pick(std::span<const double> weights, Stochastic& generator)
    {
        double total = 0.0;
        for (double weight : weights) {
            total += std::max(0.0, weight);
        }

        if (total <= 0.0) {
            return weights.size();
        }

        const double target = generator(0.0, total);
        double running = 0.0;
        for (size_t i = 0; i < weights.size(); ++i) {
            running += std::max(0.0, weights[i]);
            if (target <= running) {
                return i;
            }
        }
        return weights.size() - 1;
    }

    /**
     * @brief Weighted random draw over one row of this table
     * @param row_index Row index, unchecked
     * @param generator Entropy source for the draw
     * @return Index into the row, or cols() when every weight in the row
     *         is zero or negative
     */
    [[nodiscard]] size_t weighted_pick(size_t row_index, Stochastic& generator) const
    {
        return weighted_pick(row(row_index), generator);
    }

private:
    size_t m_rows {};
    size_t m_cols {};
    std::vector<double> m_data;
};

} // namespace MayaFlux::Kinesis::Stochastic
