#pragma once

#include "UniversalSorter.hpp"

#include "MayaFlux/Yantra/Analyzers/VisionAnalyzer.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"

namespace MayaFlux::Yantra {

/**
 * @enum VisionSortKey
 * @brief Named scalar attributes on vision detection results used as sort keys.
 */
enum class VisionSortKey : uint8_t {
    AREA,
    PERIMETER,
    CENTROID_X,
    CENTROID_Y,
    WIDTH,
    HEIGHT,
};

/**
 * @class VisionSorter
 * @brief Reorders detection results in a VisionAnalysis by a scalar key or callable.
 *
 * Reads VisionAnalysis from input.metadata["vision_analysis"], sorts the
 * populated detection collection (boxes or contours) by the configured key
 * and direction, writes the reordered VisionAnalysis back to
 * output.metadata["vision_analysis"], and passes input.data through unmodified.
 *
 * CPU path: std::stable_sort via permutation index.
 *
 * GPU path: attach a ShaderExecutionContext configured with
 * KernelTemplate::BitonicSort. apply_operation_internal computes scalar keys
 * on CPU, stages them alongside a linear index array, runs the assembled
 * bitonic sort shader, reads back sorted indices, and applies the permutation
 * to the analysis collections before returning.
 *
 * The callable overload receives the element index and the full VisionAnalysis.
 * When both a key and a callable are set, the callable takes precedence.
 *
 * @note The GPU backend must be a ShaderExecutionContext<> with default
 *       InputType/OutputType (vector<DataVariant>). The spec must be built
 *       with KernelTemplate::BitonicSort, two InOut FLOAT32 SSBOs (keys at
 *       binding 0, indices at binding 1), and four UINT32 PC fields in order:
 *       stage, pass, count, descending.
 *
 * @code
 * auto spec = ShaderSpec::Assemble{}
 *     .tmpl(KernelTemplate::BitonicSort)
 *     .ssbo("keys",    BindingDirection::InOut, GpuDataFormat::FLOAT32)
 *     .ssbo("indices", BindingDirection::InOut, GpuDataFormat::FLOAT32)
 *     .pc("stage",      GpuDataFormat::UINT32)
 *     .pc("pass",       GpuDataFormat::UINT32)
 *     .pc("count",      GpuDataFormat::UINT32)
 *     .pc("descending", GpuDataFormat::UINT32)
 *     .workgroup(256)
 *     .build();
 * auto ctx = std::make_shared<ShaderExecutionContext<>>(
 *     config_from_spec(spec), bindings_from_spec(spec));
 * sorter->set_gpu_backend(ctx);
 * @endcode
 *
 * @tparam InputType  Any ComputeData type. Defaults to shared_ptr<SignalSourceContainer>.
 * @tparam OutputType Defaults to InputType; data passes through unmodified.
 */
template <
    ComputeData InputType = std::shared_ptr<Kakshya::SignalSourceContainer>,
    ComputeData OutputType = InputType>
class VisionSorter
    : public UniversalSorter<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;

    using KeyFn = std::function<float(size_t index, const VisionAnalysis&)>;

    explicit VisionSorter(
        VisionSortKey key = VisionSortKey::AREA,
        SortingDirection direction = SortingDirection::DESCENDING)
        : m_key(key)
    {
        this->set_direction(direction);
        this->set_strategy(SortingStrategy::COPY_SORT);
        this->set_granularity(SortingGranularity::RAW_DATA);
    }

    explicit VisionSorter(
        KeyFn key_fn,
        SortingDirection direction = SortingDirection::DESCENDING)
        : m_key_fn(std::move(key_fn))
    {
        this->set_direction(direction);
        this->set_strategy(SortingStrategy::COPY_SORT);
        this->set_granularity(SortingGranularity::RAW_DATA);
    }

    void set_key(VisionSortKey key)
    {
        m_key = key;
        m_key_fn = nullptr;
    }
    void set_key_fn(KeyFn key_fn) { m_key_fn = std::move(key_fn); }

    [[nodiscard]] VisionSortKey get_key() const { return m_key; }
    [[nodiscard]] bool has_key_fn() const { return m_key_fn != nullptr; }

    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::SPATIAL;
    }

    [[nodiscard]] std::vector<std::string> get_available_methods() const
    {
        return Reflect::get_enum_names_lowercase<VisionSortKey>();
    }

protected:
    [[nodiscard]] std::string get_sorter_name() const override
    {
        return "VisionSorter";
    }

    output_type apply_operation_internal(
        const input_type& input, const ExecutionContext& context) override
    {
        if (this->m_gpu_backend && this->m_gpu_backend->ensure_gpu_ready()) {
            const auto analysis_it = input.metadata.find("vision_analysis");
            if (analysis_it != input.metadata.end() && analysis_it->second.has_value()) {
                auto analysis = safe_any_cast_or_throw<VisionAnalysis>(analysis_it->second);

                const size_t n = !analysis.frame.boxes.empty()
                    ? analysis.frame.boxes.size()
                    : analysis.frame.contours.size();

                if (n > 1) {
                    uint32_t padded = 1;
                    while (padded < static_cast<uint32_t>(n))
                        padded <<= 1;

                    std::vector<float> keys(padded, std::numeric_limits<float>::infinity());
                    for (size_t i = 0; i < n; ++i)
                        keys[i] = compute_key(i, analysis);

                    std::vector<float> indices(padded);
                    for (uint32_t i = 0; i < padded; ++i)
                        indices[i] = static_cast<float>(i);

                    const auto k = static_cast<uint32_t>(
                        std::ceil(std::log2(static_cast<double>(padded))));
                    const uint32_t total_passes = k * (k + 1) / 2;
                    const uint32_t desc = (this->get_direction() == SortingDirection::DESCENDING) ? 1U : 0U;
                    const uint32_t count = padded;

                    auto* ctx = dynamic_cast<ShaderExecutionContext<>*>(
                        this->m_gpu_backend.get());

                    if (ctx) {
                        ctx->in_out(keys).in_out(indices);
                        ctx->set_multipass(total_passes,
                            [k, desc, count](uint32_t p, void* pc_ptr) {
                                uint32_t stage = 0, pass = 0, remaining = p;
                                for (uint32_t s = 0; s < k; ++s) {
                                    if (remaining <= s) {
                                        stage = s;
                                        pass = remaining;
                                        break;
                                    }
                                    remaining -= (s + 1);
                                }
                                struct PC {
                                    uint32_t stage, pass, count, descending;
                                };
                                *static_cast<PC*>(pc_ptr) = { stage, pass, count, desc };
                            });

                        auto* ctx = dynamic_cast<ShaderExecutionContext<InputType, OutputType>*>(
                            this->m_gpu_backend.get());
                        auto gpu_result = this->m_gpu_backend->execute(input, context);

                        std::vector<float> sorted_idx_floats(padded);
                        ctx->download_binding(1, sorted_idx_floats.data(),
                            padded * sizeof(float));

                        auto apply_permutation = [&]<typename T>(std::vector<T>& coll) {
                            std::vector<T> sorted(n);
                            for (size_t i = 0; i < n; ++i) {
                                const auto src = static_cast<size_t>(sorted_idx_floats[i]);
                                if (src < n)
                                    sorted[i] = coll[src];
                            }
                            coll = std::move(sorted);
                        };

                        if (!analysis.frame.boxes.empty())
                            apply_permutation(analysis.frame.boxes);
                        if (!analysis.frame.contours.empty())
                            apply_permutation(analysis.frame.contours);

                        output_type out { input.data };
                        out.metadata = input.metadata;
                        out.metadata["vision_analysis"] = std::move(analysis);
                        return out;
                    }
                }
            }
        }

        return UniversalSorter<InputType, OutputType>::apply_operation_internal(input, context);
    }

    output_type sort_implementation(const input_type& input) override
    {
        const auto analysis_it = input.metadata.find("vision_analysis");
        if (analysis_it == input.metadata.end() || !analysis_it->second.has_value()) {
            error<std::runtime_error>(
                Journal::Component::Yantra,
                Journal::Context::ComputeMatrix,
                std::source_location::current(),
                "VisionSorter: no vision_analysis in input metadata");
        }

        auto analysis = safe_any_cast_or_throw<VisionAnalysis>(analysis_it->second);

        const bool descending = (this->get_direction() == SortingDirection::DESCENDING);

        auto sort_collection = [&]<typename T>(std::vector<T>& coll) {
            const size_t n = coll.size();
            std::vector<size_t> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::stable_sort(idx.begin(), idx.end(),
                [&](size_t a, size_t b) {
                    const float ka = m_key_fn
                        ? m_key_fn(a, analysis)
                        : key_for(a, coll, analysis);
                    const float kb = m_key_fn
                        ? m_key_fn(b, analysis)
                        : key_for(b, coll, analysis);
                    return descending ? ka > kb : ka < kb;
                });
            std::vector<T> sorted(n);
            for (size_t i = 0; i < n; ++i)
                sorted[i] = coll[idx[i]];
            coll = std::move(sorted);
        };

        if (!analysis.frame.boxes.empty())
            sort_collection(analysis.frame.boxes);
        if (!analysis.frame.contours.empty())
            sort_collection(analysis.frame.contours);

        output_type out { input.data };
        out.metadata = input.metadata;
        out.metadata["vision_analysis"] = std::move(analysis);
        return out;
    }

private:
    VisionSortKey m_key { VisionSortKey::AREA };
    KeyFn m_key_fn;

    [[nodiscard]] float compute_key(size_t index, const VisionAnalysis& analysis) const
    {
        if (m_key_fn)
            return m_key_fn(index, analysis);
        if (!analysis.frame.boxes.empty())
            return key_for_box(analysis.frame.boxes[index]);
        if (!analysis.frame.contours.empty())
            return key_for_contour(analysis.frame.contours[index]);
        return 0.F;
    }

    template <typename T>
    [[nodiscard]] float key_for(size_t index, const std::vector<T>& coll,
        const VisionAnalysis& analysis) const
    {
        if constexpr (std::is_same_v<T, Kinesis::Vision::BoundingBox>) {
            return key_for_box(coll[index]);
        } else {
            return key_for_contour(coll[index]);
        }
        (void)analysis;
    }

    [[nodiscard]] float key_for_box(const Kinesis::Vision::BoundingBox& box) const
    {
        switch (m_key) {
        case VisionSortKey::AREA:
            return box.w * box.h;
        case VisionSortKey::PERIMETER:
            return 2.F * (box.w + box.h);
        case VisionSortKey::CENTROID_X:
            return box.x + box.w * 0.5F;
        case VisionSortKey::CENTROID_Y:
            return box.y + box.h * 0.5F;
        case VisionSortKey::WIDTH:
            return box.w;
        case VisionSortKey::HEIGHT:
            return box.h;
        }
        return 0.F;
    }

    [[nodiscard]] float key_for_contour(const Kinesis::Vision::Contour& contour) const
    {
        switch (m_key) {
        case VisionSortKey::AREA:
            return contour.area;
        case VisionSortKey::PERIMETER:
            return contour.perimeter;
        case VisionSortKey::WIDTH:
        case VisionSortKey::HEIGHT:
        case VisionSortKey::CENTROID_X:
        case VisionSortKey::CENTROID_Y: {
            if (contour.points.empty())
                return 0.F;
            glm::vec2 mn = contour.points[0], mx = contour.points[0];
            for (const auto& p : contour.points) {
                if (p.x < mn.x)
                    mn.x = p.x;
                if (p.y < mn.y)
                    mn.y = p.y;
                if (p.x > mx.x)
                    mx.x = p.x;
                if (p.y > mx.y)
                    mx.y = p.y;
            }
            if (m_key == VisionSortKey::WIDTH)
                return mx.x - mn.x;
            if (m_key == VisionSortKey::HEIGHT)
                return mx.y - mn.y;
            if (m_key == VisionSortKey::CENTROID_X)
                return (mn.x + mx.x) * 0.5F;
            return (mn.y + mx.y) * 0.5F;
        }
        }
        return 0.F;
    }
};

// ============================================================================
// Aliases
// ============================================================================

using StandardVisionSorter = VisionSorter<
    std::shared_ptr<Kakshya::SignalSourceContainer>,
    std::shared_ptr<Kakshya::SignalSourceContainer>>;

using DataVisionSorter = VisionSorter<
    std::vector<Kakshya::DataVariant>,
    std::vector<Kakshya::DataVariant>>;

} // namespace MayaFlux::Yantra
