#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "BitonicSorter.hpp"
#include "Sorter.hpp"

/**
 * @file RegionKeySorter.hpp
 * @brief Orders region groups by a weighted score of their feature facets.
 */

namespace MayaFlux::Yantra {

/**
 * @struct RegionKeySorterConfig
 * @brief Settings of RegionKeySorter, stored whole in the operation's facets.
 *
 * The keys are the terms of the score. The GPU threshold selects the dispatch path.
 */
struct RegionKeySorterConfig : SorterConfig {
    /** Terms of the weighted sort score. Weights are validated on construction. */
    std::vector<SortKey> keys;

    /** Region count at or above which the GPU path is used. 0 keeps the sort on the CPU. */
    uint32_t gpu_sort_threshold { 0 };
};

MF_DESCRIBE(RegionKeySorterConfig)

/**
 * @class RegionKeySorter
 * @brief Orders regions by their weighted sort score.
 *
 * Each region's score is weighted_sort_score over the configured keys. Regions
 * are ordered by ascending score. A DESCENDING key negates its term, so
 * direction lives in the keys. Empty keys leave the group unchanged. A region
 * that lacks any key's feature is a hard error.
 *
 * The GPU path orders the scores with BitonicSorter over order keys. It returns
 * the same permutation as the CPU path: ascending, with equal scores in input
 * order and NaN last.
 */
class RegionKeySorter : public Sorter<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
    using supports = Supports<RegionKeySorterConfig>;

    /**
     * @brief Construct a region key sorter from its settings.
     * @param config Keys and GPU threshold. Key weights are validated.
     */
    explicit RegionKeySorter(RegionKeySorterConfig config = {})
    {
        validate_sort_keys(config.keys);
        set_config(std::move(config));
    }

    /**
     * @brief The stored settings, or defaults when none were set.
     */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return stored_configs(supports {});
    }

    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::ALGORITHMIC;
    }

protected:
    Datum<Kakshya::RegionGroup> sort_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;

private:
    /**
     * @brief Ascending order of the scores, computed exactly on the GPU.
     */
    static std::vector<size_t> gpu_order(const std::vector<double>& scores);
};

inline Datum<Kakshya::RegionGroup> RegionKeySorter::sort_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    const auto& cfg = config<RegionKeySorterConfig>();
    Datum<Kakshya::RegionGroup> out = input;
    const auto n = out.data.regions.size();

    if (n == 0 || cfg.keys.empty()) {
        return out;
    }

    std::vector<double> scores(n);
    for (size_t i = 0; i < n; ++i) {
        const auto score = weighted_sort_score(out.data.regions[i], cfg.keys);
        if (!score) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix, std::source_location::current(), "RegionKeySorter: a region lacks a sort key feature");
        }
        scores[i] = *score;
    }

    std::vector<size_t> order(n);
    if (cfg.gpu_sort_threshold > 0 && n >= cfg.gpu_sort_threshold) {
        order = gpu_order(scores);
    } else {
        std::iota(order.begin(), order.end(), size_t { 0 });
        std::ranges::stable_sort(order, [&](size_t a, size_t b) { return scores[a] < scores[b]; });
    }

    std::vector<Kakshya::Region> reordered;
    reordered.reserve(n);
    for (size_t idx : order) {
        reordered.push_back(out.data.regions[idx]);
    }
    out.data.regions = std::move(reordered);
    out.data.current_region_index = 0;
    out.data.active_indices.clear();
    return out;
}

inline std::vector<size_t> RegionKeySorter::gpu_order(const std::vector<double>& scores)
{
    StandardSorterConfig config;
    config.direction = SortingDirection::ASCENDING;
    BitonicSorter<> sorter(config);
    return sorter.order(scores);
}

} // namespace MayaFlux::Yantra
#include "RegionKeySorter.describe.hpp"
