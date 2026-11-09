#pragma once

#include "ShaderSpecBinding.hpp"
#include "WordPairExecutionContext.hpp"

/**
 * @file SegmentedReductionContext.hpp
 * @brief Reduces segments of doubles on the GPU, exactly, with no fp64 on the device.
 */

namespace MayaFlux::Yantra {

/**
 * @struct SegmentGeometry
 * @brief Where the segments of a reduction sit in the value array.
 *
 * Segment s covers length elements starting at offset + s * stride. A stride
 * equal to length tiles the array; a smaller stride overlaps segments.
 */
struct SegmentGeometry {
    uint32_t length { 0 };
    uint32_t stride { 0 };
    uint32_t offset { 0 };
    uint32_t count { 0 };
};

/**
 * @class SegmentedReductionContext
 * @brief One reduction per segment, as one dispatch of the SegmentedReduction template.
 *
 * Values are staged as word pairs and combined by the template's emulated
 * IEEE double arithmetic, so a result agrees with a CPU reduction up to the
 * order the partial results are combined in. The map is applied to each value
 * before the combine. Max and Min follow the library order key, where every
 * NaN is the largest. An empty segment yields the combine's identity: zero for
 * Sum, negative infinity for Max, NaN for Min.
 */
class SegmentedReductionContext : public WordPairExecutionContext {
public:
    /**
     * @param combine Sum, Max or Min.
     * @param map Pointwise map applied before the combine.
     * @param workgroup Invocations per segment. A power of two, at least 2.
     */
    SegmentedReductionContext(
        Portal::Graphics::KernelOp combine,
        Portal::Graphics::ReductionMap map = Portal::Graphics::ReductionMap::Identity,
        uint32_t workgroup = 256)
        : SegmentedReductionContext(reduction_spec(combine, map, workgroup))
    {
    }

    /**
     * @brief Reduce every segment.
     * @param values Doubles the segments index into.
     * @param geometry Segment length, stride, first offset and count. Every segment must lie inside values.
     * @return One double per segment, empty when there are no segments.
     */
    [[nodiscard]] std::vector<double> reduce(std::span<const double> values, const SegmentGeometry& geometry)
    {
        if (geometry.count == 0) {
            return {};
        }
        m_segments = geometry.count;

        stage_words(0, values);
        set_output_size(1, static_cast<size_t>(geometry.count) * sizeof(uint64_t));
        push(Push { .length = geometry.length, .stride = geometry.stride, .offset = geometry.offset });
        execute(Datum<std::vector<Kakshya::DataVariant>> {}, ExecutionContext {});

        const auto result = primary_values();
        return { result.begin(), result.end() };
    }

    /**
     * @brief The spec of the template instance this context drives.
     */
    [[nodiscard]] static Portal::Graphics::ShaderSpec reduction_spec(
        Portal::Graphics::KernelOp combine, Portal::Graphics::ReductionMap map, uint32_t workgroup)
    {
        using Portal::Graphics::BindingDirection;
        return Portal::Graphics::ShaderSpec::Assemble {}
            .tmpl(Portal::Graphics::KernelTemplate::SegmentedReduction)
            .op(combine)
            .map(map)
            .ssbo("values", BindingDirection::Input, Kakshya::GpuDataFormat::VEC2_U32)
            .ssbo("result", BindingDirection::Output, Kakshya::GpuDataFormat::VEC2_U32)
            .pc("length", Kakshya::GpuDataFormat::UINT32)
            .pc("stride", Kakshya::GpuDataFormat::UINT32)
            .pc("offset", Kakshya::GpuDataFormat::UINT32)
            .workgroup(workgroup)
            .build();
    }

protected:
    /**
     * @brief One workgroup per segment.
     */
    [[nodiscard]] std::array<uint32_t, 3> calculate_dispatch_size(
        size_t /*total_elements*/, const DataStructureInfo& /*structure_info*/) const override
    {
        return { m_segments, 1U, 1U };
    }

private:
    struct Push {
        uint32_t length;
        uint32_t stride;
        uint32_t offset;
    };

    explicit SegmentedReductionContext(const Portal::Graphics::ShaderSpec& spec)
        : WordPairExecutionContext(config_from_spec(spec), bindings_from_spec(spec), false, "SegmentedReductionContext")
    {
    }

    uint32_t m_segments { 1 };
};

} // namespace MayaFlux::Yantra
