#pragma once

#include "StandardSorter.hpp"

#include "MayaFlux/Yantra/Executors/ShaderSpecBinding.hpp"
#include "MayaFlux/Yantra/Executors/WordPairExecutionContext.hpp"

/**
 * @file BitonicSorter.hpp
 * @brief Exact GPU sorting of numeric channels by a bitonic network over order keys.
 */

namespace MayaFlux::Yantra {

/**
 * @class BitonicSorter
 * @brief Sorts each numeric channel on the GPU, exactly, without fp64.
 *
 * The GPU candidate for the settings StandardSorter takes. Values are ordered
 * through their order keys, so every double sorts exactly and the result
 * matches a stable CPU sort: equal values keep their input order, -0.0 equals
 * +0.0 and NaN sorts last, or first when descending. Every SortingAlgorithm is
 * satisfied by the full sort, as are the in place, partial, parallel and index
 * strategies, and the result is a sorted copy. The bidirectional order is the
 * ascending sort laid out by Kinesis mountain_positions, and chunked sorting
 * sorts each chunk on its own and concatenates them, as the CPU sorter does.
 * order() gives the permutation of one channel.
 *
 * A custom comparator is host code and cannot run in a shader, so
 * validate_input declines it and another candidate takes the call. fitness
 * grows with the channel length and is zero while no device is up.
 */
template <ComputeData OutputType = std::vector<Kakshya::DataVariant>>
class BitonicSorter : public Sorter<std::vector<Kakshya::DataVariant>, OutputType> {
public:
    using base_type = Sorter<std::vector<Kakshya::DataVariant>, OutputType>;
    using input_type = typename base_type::input_type;
    using output_type = typename base_type::output_type;
    using supports = Supports<StandardSorterConfig>;

    static constexpr Target target = targets::vulkan;

    /**
     * @brief Channel length at which this candidate scores the same as a CPU sorter.
     */
    static constexpr size_t fitness_scale = 4096;

    explicit BitonicSorter(StandardSorterConfig config = {})
    {
        this->set_config(std::move(config));
    }

    [[nodiscard]] ConfigRefs configs() const override
    {
        return this->stored_configs(supports {});
    }

    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::ALGORITHMIC;
    }

    /**
     * @brief Longest channel over fitness_scale, or zero while no device is up.
     */
    static float fitness(const input_type& input)
    {
        if (!Portal::Graphics::get_shader_foundry().is_initialized()) {
            return 0.0F;
        }
        size_t longest = 0;
        for (const auto& channel : input.data) {
            longest = std::max(longest, std::visit([](const auto& values) { return values.size(); }, channel));
        }
        return static_cast<float>(longest) / static_cast<float>(fitness_scale);
    }

    /**
     * @brief False for a custom comparator, which cannot run on the device, and for chunked sorting with no chunk size.
     */
    bool validate_input(const input_type& /*input*/) const override
    {
        const auto& config = this->template config<StandardSorterConfig>();
        const bool chunk_ok = config.strategy != SortingStrategy::CHUNKED_SORT || config.chunk_size > 0;
        return config.direction != SortingDirection::CUSTOM && chunk_ok;
    }

    /**
     * @brief The indices that order one channel, computed on the GPU in the configured direction.
     * @return result[i] is the index in values of the i-th value in order. For the
     *         bidirectional order, result[i] is the value placed at position i.
     */
    [[nodiscard]] std::vector<size_t> order(std::span<const double> values)
    {
        const auto direction = this->template config<StandardSorterConfig>().direction;
        const bool descending = direction == SortingDirection::DESCENDING;
        const size_t count = values.size();
        std::vector<size_t> result(count);
        std::iota(result.begin(), result.end(), size_t { 0 });
        if (count < 2) {
            return result;
        }

        auto& executor = this->context();
        const size_t padded = std::bit_ceil(count);
        executor.stage_words(k_keys_binding, values, padded, descending);

        std::vector<uint32_t> indices(padded);
        std::iota(indices.begin(), indices.end(), uint32_t { 0 });
        executor.set_binding_data(k_indices_binding, indices);

        const auto stages = static_cast<uint32_t>(std::countr_zero(padded));
        executor.set_multipass(stages * (stages + 1U) / 2U,
            [padded](uint32_t pass_index, void* push) {
                uint32_t level = 0;
                uint32_t remaining = pass_index;
                while (remaining > level) {
                    remaining -= level + 1U;
                    ++level;
                }
                const SortPush pc {
                    .stage = level + 1U,
                    .pass = level - remaining,
                    .count = static_cast<uint32_t>(padded),
                    .descending = 0U,
                };
                std::memcpy(push, &pc, sizeof(SortPush));
            });

        executor.execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});

        const auto sorted = executor.primary_words();
        if (sorted.size() < count) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::BufferProcessing,
                std::source_location::current(), "BitonicSorter: the sort returned {} of {} indices",
                sorted.size(), count);
        }
        std::copy_n(sorted.begin(), count, result.begin());

        if (direction == SortingDirection::BIDIRECTIONAL) {
            const auto positions = Kinesis::Discrete::mountain_positions(count);
            std::vector<size_t> placed(count);
            for (size_t rank = 0; rank < count; ++rank) {
                placed[positions[rank]] = result[rank];
            }
            return placed;
        }
        return result;
    }

protected:
    output_type sort_implementation(const input_type& input) override
    {
        std::vector<std::vector<double>> working;
        auto [channels, structure_info] = OperationHelper::setup_operation_buffer(
            const_cast<input_type&>(input), working);

        const auto& config = this->template config<StandardSorterConfig>();
        const bool chunked = config.strategy == SortingStrategy::CHUNKED_SORT;
        for (auto& channel : channels) {
            const size_t segment = std::max<size_t>(chunked ? config.chunk_size : channel.size(), 1);
            for (size_t start = 0; start < channel.size(); start += segment) {
                arrange(channel.subspan(start, std::min(segment, channel.size() - start)));
            }
        }

        output_type result = this->convert_result(working, structure_info);
        result.metadata = input.metadata;
        result.facets = input.facets;
        record_sort(result, chunked ? SortPath::CHUNKED_MERGED : SortPath::COPY);
        return result;
    }

private:
    struct SortPush {
        uint32_t stage;
        uint32_t pass;
        uint32_t count;
        uint32_t descending;
    };

    static constexpr uint32_t k_keys_binding = 0;
    static constexpr uint32_t k_indices_binding = 1;

    /**
     * @brief Reorders a segment in place by its GPU permutation.
     */
    void arrange(std::span<double> segment)
    {
        const auto permutation = order(segment);
        const std::vector<double> original(segment.begin(), segment.end());
        for (size_t i = 0; i < permutation.size(); ++i) {
            segment[i] = original[permutation[i]];
        }
    }

    void record_sort(output_type& out, SortPath path) const
    {
        SortRecord record;
        record.config = this->template config<StandardSorterConfig>();
        record.path = path;
        out.facets.set(record);
    }

    /**
     * @brief The context, built on first use so a sorter can be created before any device.
     */
    WordPairExecutionContext& context()
    {
        if (!m_context) {
            const auto spec = sort_spec();
            auto bindings = bindings_from_spec(spec);
            std::ranges::rotate(bindings, bindings.begin() + 1);
            m_context = std::make_shared<WordPairExecutionContext>(
                config_from_spec(spec), std::move(bindings), true, "BitonicSorter");
        }
        return *m_context;
    }

    static Portal::Graphics::ShaderSpec sort_spec()
    {
        using Portal::Graphics::BindingDirection;
        using Portal::Graphics::KernelTemplate;
        return Portal::Graphics::ShaderSpec::Assemble {}
            .tmpl(KernelTemplate::BitonicSort)
            .ssbo("keys", BindingDirection::InOut, Kakshya::GpuDataFormat::VEC2_U32)
            .ssbo("indices", BindingDirection::InOut, Kakshya::GpuDataFormat::UINT32)
            .pc("stage", Kakshya::GpuDataFormat::UINT32)
            .pc("pass", Kakshya::GpuDataFormat::UINT32)
            .pc("count", Kakshya::GpuDataFormat::UINT32)
            .pc("descending", Kakshya::GpuDataFormat::UINT32)
            .build();
    }

    std::shared_ptr<WordPairExecutionContext> m_context;
};

} // namespace MayaFlux::Yantra
