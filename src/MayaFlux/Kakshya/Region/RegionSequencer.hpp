#pragma once

#include "OrganizedRegion.hpp"
#include "RegionGroup.hpp"

#include "MayaFlux/Kinesis/Stochastic/Stochastic.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class RegionSequencer
 * @brief Chooses which region of a group comes next, by the group's selection pattern.
 *
 * Pure decision logic with no container and no clock. It owns the one position
 * in the walk, the index of the current region, and keeps a runtime record
 * (OrganizedRegion) per region with the group's and the region's facets. The
 * order of the group is the order of the walk. Random choices are drawn from a
 * seeded Kinesis generator, so a seed repeats a walk exactly.
 *
 * Patterns:
 * - SEQUENTIAL: group order, ending after the last region.
 * - ROUND_ROBIN: group order, wrapping.
 * - RANDOM: uniform over the group.
 * - WEIGHTED: drawn by a Facet<double> read from each region, 1 when absent.
 * - EXCLUSIVE: stays on the group's current_region_index until the group is replaced.
 * - CUSTOM: chosen by the supplied Chooser.
 *
 * ALL and OVERLAP, and the CROSSFADE and OVERLAP transitions, are not
 * supported; unsupported_reason names them and the constructor rejects them.
 */
class MAYAFLUX_API RegionSequencer {
public:
    /**
     * @brief Picks the next region for CUSTOM. Receives the group and the current index.
     * @return The index to play, or nothing to end the walk.
     */
    using Chooser = std::function<std::optional<size_t>(const RegionGroup&, std::optional<size_t>)>;

    /**
     * @param group Group to walk, copied.
     * @param seed Seed of the random patterns.
     * @throws std::invalid_argument if the group uses an unsupported pattern or transition.
     */
    explicit RegionSequencer(RegionGroup group, uint64_t seed = 0);

    /**
     * @brief Why a group cannot be sequenced, or an empty string if it can.
     */
    [[nodiscard]] static std::string unsupported_reason(const RegionGroup& group);

    /**
     * @brief Read WEIGHTED weights from this key.
     */
    void set_weight_key(Facet<double> key);

    void set_chooser(Chooser chooser);

    /**
     * @brief The next region by the pattern, which becomes the current one.
     * @return Its index, or nothing when the walk has ended: after the last region
     *         for SEQUENTIAL, when every weight is zero, or when the chooser ends it.
     *         An ended walk stays ended until reset.
     */
    [[nodiscard]] std::optional<size_t> next();

    /**
     * @brief Make @p index the current region.
     * @throws std::out_of_range if there is no such region.
     */
    void jump(size_t index);

    /**
     * @brief Start the walk again with the seed.
     */
    void reset();

    [[nodiscard]] std::optional<size_t> current() const noexcept { return m_current; }
    [[nodiscard]] const RegionGroup& group() const noexcept { return m_group; }
    [[nodiscard]] const std::vector<OrganizedRegion>& regions() const noexcept { return m_regions; }

private:
    void choose(size_t index);
    void refresh_weights();

    RegionGroup m_group;
    uint64_t m_seed;
    Kinesis::Stochastic::Stochastic m_generator;
    std::vector<OrganizedRegion> m_regions;
    std::vector<double> m_weights;
    std::optional<Facet<double>> m_weight_key;
    Chooser m_chooser;
    std::optional<size_t> m_current;
};

}
