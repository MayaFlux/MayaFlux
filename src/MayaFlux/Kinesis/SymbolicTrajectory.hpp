#pragma once

#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"
#include "MayaFlux/Transitive/Memory/RingBuffer.hpp"

namespace MayaFlux::Kinesis {

/**
 * @class SymbolicTrajectory
 * @brief Tracks a moving point's sequence of cells through a lattice
 *        partition over time.
 *
 * Named for symbolic dynamics: a continuous trajectory, observed through
 * a partition of its space into discrete cells, becomes a sequence of
 * symbols, one per cell occupied at each observation. This class is that
 * observation process, holding the memory a single Lattice2D::cell_at()
 * or Lattice3D::cell_at() call does not: which cell the point occupied
 * last, how many times it has crossed a cell boundary, and a short
 * window of recently visited cells for measuring the shape of that
 * crossing sequence.
 *
 * Distinct from SpatialIndex, which tracks the current positions of
 * many entities for neighbor queries and has no notion of history, and
 * from Lattice2D/Lattice3D themselves, which are pure static geometry
 * with no notion of a point moving through them at all. SymbolicTrajectory
 * adds no new geometry: cell containment is entirely LatticeT::cell_at().
 * It adds only memory of what cell_at() has returned over time for one
 * moving point.
 *
 * @tparam LatticeT Lattice2D or Lattice3D
 * @tparam CellT The corresponding cell coordinate type: glm::uvec2 for
 *         Lattice2D, glm::uvec3 for Lattice3D. Not deduced automatically
 *         since LatticeT does not expose its own coordinate type as a
 *         nested alias; specify explicitly at the call site.
 *
 * ```cpp
 * SymbolicTrajectory<Lattice2D, glm::uvec2> traj(Lattice2D::ndc_quadrants(), 16);
 * for (auto pos : incoming_positions) {
 *     traj.update(pos);
 * }
 * size_t crossings = traj.crossing_count();
 * size_t distinct = traj.unique_cells_in_window(8);
 * ```
 */
template <typename LatticeT, typename CellT>
class SymbolicTrajectory {
public:
    /**
     * @brief Construct a trajectory over a given lattice
     * @param lattice The partition to observe the incoming positions through
     * @param window Number of recent cell observations to retain for
     *        windowed queries (unique_cells_in_window, crossings_in_window,
     *        dominant_cell). Minimum 2.
     */
    explicit SymbolicTrajectory(LatticeT lattice, size_t window = 16)
        : m_lattice(lattice)
        , m_history(window < 2 ? 2 : window)
    {
    }

    /**
     * @brief Observe one new position, updating the cell sequence
     * @tparam PositionT glm::vec2 for a Lattice2D-backed trajectory,
     *         glm::vec3 for a Lattice3D-backed trajectory; must match
     *         what LatticeT::cell_at accepts
     * @param position Position in the same coordinate space as the lattice
     * @return true if this observation crossed into a different cell
     *         than the previous observation, false if it stayed in the
     *         same cell or this is the first observation
     */
    template <typename PositionT>
    bool update(const PositionT& position)
    {
        const CellT cell = m_lattice.cell_at(position);
        m_history.push(cell);

        if (!m_has_prior) {
            m_has_prior = true;
            m_current_cell = cell;
            return false;
        }

        const bool crossed = !(cell == m_current_cell);
        if (crossed) {
            ++m_crossing_count;
            m_current_cell = cell;
        }
        return crossed;
    }

    /**
     * @brief Cell the most recent observation fell in
     * @pre At least one update() call has happened
     */
    [[nodiscard]] const CellT& current_cell() const { return m_current_cell; }

    /**
     * @brief Total crossings observed since construction or reset()
     *
     * A crossing is any observation whose cell differs from the
     * immediately preceding observation's cell, regardless of whether
     * the sequence later returns to a previously visited cell.
     */
    [[nodiscard]] size_t crossing_count() const { return m_crossing_count; }

    /**
     * @brief Crossings within the most recent @p window observations
     * @param window Number of recent observations to examine, clamped
     *        to the trajectory's retained history capacity
     * @return Count of adjacent-pair differences within the window
     *
     * Distinct from crossing_count(): this only looks at the retained
     * window, so it answers "how much boundary-crossing has happened
     * recently" rather than "how much has happened ever."
     */
    [[nodiscard]] size_t crossings_in_window(size_t window) const
    {
        window = std::min(window, m_history.capacity());
        if (window < 2)
            return 0;

        const auto view = m_history.linearized_view();
        size_t crossings = 0;
        for (size_t i = 0; i + 1 < window; ++i) {
            if (!(view[i] == view[i + 1]))
                ++crossings;
        }
        return crossings;
    }

    /**
     * @brief Count of distinct cells visited within the most recent
     *        @p window observations
     * @param window Number of recent observations to examine, clamped
     *        to the trajectory's retained history capacity
     * @return Number of unique cells, 1 if the window never left one
     *         cell, up to window if every observation was a new cell
     *
     * Distinguishes a trajectory pacing back and forth between two
     * cells (high crossings_in_window, low unique_cells_in_window) from
     * one sweeping steadily through new territory (both high).
     */
    [[nodiscard]] size_t unique_cells_in_window(size_t window) const
    {
        window = std::min(window, m_history.capacity());
        if (window == 0)
            return 0;

        const auto view = m_history.linearized_view();
        std::vector<CellT> seen;
        seen.reserve(window);
        for (size_t i = 0; i < window; ++i) {
            bool found = false;
            for (const auto& s : seen) {
                if (s == view[i]) {
                    found = true;
                    break;
                }
            }
            if (!found)
                seen.push_back(view[i]);
        }
        return seen.size();
    }

    /**
     * @brief Cell with the most observations within the most recent
     *        @p window observations
     * @param window Number of recent observations to examine, clamped
     *        to the trajectory's retained history capacity
     * @return The most-occupied cell in the window, and how many of the
     *         window's observations fell in it
     *
     * Ties resolve to whichever qualifying cell appears first in the
     * scan, which is the most recent one among ties since the
     * underlying HistoryBuffer is newest-first; not documented as a
     * guarantee beyond "deterministic," since which specific tie-break
     * rule matters is a caller decision this does not presume to make.
     */
    [[nodiscard]] std::pair<CellT, size_t> dominant_cell(size_t window) const
    {
        window = std::min(window, m_history.capacity());
        const auto view = m_history.linearized_view();

        CellT best {};
        size_t best_count = 0;
        for (size_t i = 0; i < window; ++i) {
            size_t count = 0;
            for (size_t j = 0; j < window; ++j) {
                if (view[j] == view[i])
                    ++count;
            }
            if (count > best_count) {
                best_count = count;
                best = view[i];
            }
        }
        return { best, best_count };
    }

    /**
     * @brief Reset to uninitialized state
     *
     * Call on a known discontinuity (the tracked point teleporting
     * rather than moving continuously) so the next update() does not
     * count a crossing against a now-meaningless prior cell.
     */
    void reset()
    {
        m_history.reset();
        m_current_cell = CellT {};
        m_has_prior = false;
        m_crossing_count = 0;
    }

    /**
     * @brief The lattice this trajectory observes positions through
     */
    [[nodiscard]] const LatticeT& lattice() const { return m_lattice; }

private:
    LatticeT m_lattice;
    Memory::HistoryBuffer<CellT> m_history;
    CellT m_current_cell {};
    bool m_has_prior { false };
    size_t m_crossing_count { 0 };
};

} // namespace MayaFlux::Kinesis
