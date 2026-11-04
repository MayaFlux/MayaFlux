#pragma once

#include "GpuSorter.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "UniversalSorter.hpp"

/**
 * @file RegionKeySorter.hpp
 * @brief Orders region groups by a weighted score of their feature facets.
 */

namespace MayaFlux::Yantra {

/**
 * @struct RegionKeySorterConfig
 * @brief Settings of RegionKeySorter, stored whole in the operation's facets.
 *
 * The keys in SorterConfig are the terms of the score. The GPU threshold
 * selects the dispatch path.
 */
struct RegionKeySorterConfig : SorterConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple_cat(
            SorterConfig::describe_facets(),
            std::tuple {
                FacetMember<&RegionKeySorterConfig::gpu_sort_threshold> {},
            });
    }

    /** Region count at or above which the GPU path is used. 0 keeps the sort on the CPU. */
    uint32_t gpu_sort_threshold { 0 };
};

/**
 * @class RegionKeySorter
 * @brief Orders regions by their weighted sort score.
 *
 * Each region's score is weighted_sort_score over the configured keys. Regions
 * are ordered by ascending score. A DESCENDING key negates its term, so
 * direction lives in the keys. Empty keys leave the group unchanged. A region
 * that lacks any key's feature is a hard error.
 *
 * The GPU path sorts the scores with the bitonic shader, which sorts one float
 * per element and so needs no change for multi-key scores.
 */
class RegionKeySorter : public UniversalSorter<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
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
    [[nodiscard]] const RegionKeySorterConfig& config() const override
    {
        if (const auto* stored = resolved_config<RegionKeySorterConfig>()) {
            return *stored;
        }
        static const RegionKeySorterConfig defaults {};
        return defaults;
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
     * @brief Ascending order of the scores, computed by the bitonic shader.
     */
    static std::vector<size_t> gpu_order(const std::vector<double>& scores);
};

inline Datum<Kakshya::RegionGroup> RegionKeySorter::sort_implementation(
    const Datum<Kakshya::RegionGroup>& input)
{
    const auto& cfg = config();
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
    const auto n = static_cast<uint32_t>(scores.size());

    struct SortPC {
        uint32_t element_count;
        uint32_t step;
        uint32_t stage;
        uint32_t ascending;
    };

    std::vector<float> values(n);
    for (uint32_t i = 0; i < n; ++i) {
        values[i] = static_cast<float>(scores[i]);
    }

    std::vector<uint32_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0U);

    /// @brief Pad element count to next power of two for bitonic correctness.
    ///        Out-of-range threads early-exit in the shader via element_count.
    uint32_t n_padded = 1U;
    while (n_padded < n) {
        n_padded <<= 1U;
    }

    /// @brief Total bitonic passes: sum of (stage+1) for stage in [0, stage_count).
    uint32_t stage_count = 0U;
    {
        uint32_t tmp = n_padded;
        while (tmp > 1U) {
            ++stage_count;
            tmp >>= 1U;
        }
    }
    const uint32_t n_passes = stage_count * (stage_count + 1U) / 2U;

    auto executor = std::make_shared<ShaderExecutionContext<>>(
        GpuComputeConfig {
            .shader_path = "sort_by_attribute.comp.spv",
            .workgroup_size = { 256, 1, 1 },
            .push_constant_size = sizeof(SortPC) });

    executor->in_out(0, values, GpuBufferBinding::ElementType::FLOAT32)
        .in_out(1, indices, GpuBufferBinding::ElementType::UINT32)
        .set_output_size(1, n * sizeof(uint32_t));

    executor->set_multipass(n_passes,
        [n](uint32_t pass, void* pc_data) {
            /// @brief Recover (stage, step) from the flat pass index.
            ///        Passes enumerate: stage=0 step=0,
            ///                          stage=1 step=0,1,
            ///                          stage=2 step=0,1,2, ...
            uint32_t stage = 0U;
            uint32_t remaining = pass;
            while (remaining >= stage + 1U) {
                remaining -= stage + 1U;
                ++stage;
            }
            const uint32_t step = stage - remaining;

            const SortPC pc {
                .element_count = n,
                .step = step,
                .stage = stage,
                .ascending = 1U,
            };
            std::memcpy(pc_data, &pc, sizeof(SortPC));
        });

    auto gpu_sorter = std::make_shared<GpuSorter<>>(executor);

    Datum<std::vector<Kakshya::DataVariant>> sort_input {
        { Kakshya::DataVariant(std::vector<double>(values.begin(), values.end())) }
    };

    auto result = gpu_sorter->apply_operation(sort_input);
    const auto sorted_indices = ShaderExecutionContext<>::read_output<uint32_t>(result, 1);

    std::vector<size_t> order(n);
    for (uint32_t i = 0; i < n; ++i) {
        order[i] = sorted_indices[i];
    }
    return order;
}

} // namespace MayaFlux::Yantra
