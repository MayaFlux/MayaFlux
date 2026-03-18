#pragma once

#include "MayaFlux/Yantra/ComputeGrammar.hpp"
#include "MayaFlux/Yantra/ComputePipeline.hpp"

#include "MayaFlux/Yantra/Analyzers/EnergyAnalyzer.hpp"
#include "MayaFlux/Yantra/Analyzers/StatisticalAnalyzer.hpp"
#include "MayaFlux/Yantra/Extractors/FeatureExtractor.hpp"
#include "MayaFlux/Yantra/Sorters/StandardSorter.hpp"

#include "MayaFlux/Kakshya/Utils/RegionUtils.hpp"

namespace MayaFlux::Kakshya {
class SoundFileContainer;
}

namespace MayaFlux::Yantra::Granular {

/**
 * @brief Datum specialisation for granular processing pipelines.
 */
using GranularDatum = Datum<Kakshya::RegionGroup>;

/**
 * @brief Datum specialisation for container-output granular pipelines.
 */
using GranularContainerDatum = Datum<std::shared_ptr<Kakshya::SignalSourceContainer>>;

/**
 * @enum GranularOutput
 * @brief Selects the terminal output type produced by make_granular_matrix.
 *
 * REGION_GROUP — pipeline terminates with a sorted RegionGroup (default).
 *               Use when you need to inspect, analyse, or further transform
 *               the grain pool before committing to audio.
 *
 * CONTAINER    — appends a reconstruct rule that stitches sorted grains into
 *               a SoundFileContainer. Use for the offline playback workflow
 *               where container-in container-out is all that is needed.
 *
 * CONTAINER_ADDITIVE — overlap-add reconstruct: grains accumulated at hop_size
 *                      intervals with optional per-grain tapering.
 */
enum class GranularOutput : uint8_t {
    REGION_GROUP,
    CONTAINER,
    CONTAINER_ADDITIVE
};

/**
 * @brief Matrix type used throughout the granular subsystem.
 */
using GranularMatrix = GrammarAwareComputeMatrix;

/**
 * @brief Span-level escape hatch for fully custom per-grain feature computation.
 *
 * Receives the raw grain samples and the current ExecutionContext.
 * Returns a single double that is written as a named Region attribute.
 * For common attribution tasks prefer the AnalysisType + qualifier path
 * via make_granular_context.
 */
using AttributeExecutor = std::function<double(std::span<const double>, const ExecutionContext&)>;

/**
 * @brief Per-grain taper applied before accumulation in OLA reconstruction.
 *
 * Receives a mutable span covering exactly one grain's samples for one channel.
 * Applied in-place before the grain is added to the output buffer.
 * Pass {} or nullptr to skip tapering entirely.
 *
 * Common use:
 *   [](std::span<double> g){ Kinesis::Discrete::apply_hann(g); }
 *   [](std::span<double> g){ Kinesis::Discrete::apply_trapezoid(g, g.size() / 8); }
 */
using GrainTaper = Kakshya::RegionTaper;

/// @brief Grammar rule executor for the reconstruction step.
extern const ComputationGrammar::Rule::Executor reconstruct_grains;

/// @brief Grammar rule executor for additive grain reconstruction.
extern const ComputationGrammar::Rule::Executor reconstruct_grains_additive;

// ============================================================================
// Concrete operations
// ============================================================================

/**
 * @class SegmentOp
 * @brief Segments container audio into fixed hop-size grain windows.
 *
 * Reads parameters from set_parameter or ExecutionContext:
 * - grain_size  uint32_t  samples per grain    (default 1024)
 * - hop_size    uint32_t  hop between grains   (default 512)
 * - channel     uint32_t  source channel index (default 0)
 */
class MAYAFLUX_API SegmentOp final
    : public FeatureExtractor<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
    SegmentOp()
        : FeatureExtractor<Kakshya::RegionGroup, Kakshya::RegionGroup>(
              1024, 512, ExtractionMethod::OVERLAPPING_WINDOWS)
    {
    }

    [[nodiscard]] ExtractionType get_extraction_type() const override
    {
        return ExtractionType::FEATURE_GUIDED;
    }

protected:
    [[nodiscard]] std::string get_extractor_name() const override
    {
        return "Granular::SegmentOp";
    }

    Datum<Kakshya::RegionGroup> extract_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;
};

/**
 * @class AttributeOp
 * @brief Computes a scalar attribute per grain and writes it onto each Region.
 *
 * Attribution is resolved in the following priority order:
 *
 * 1. Span lambda : set via set_parameter("attribute_executor", AttributeExecutor)
 *    or present in ExecutionContext. For fully custom math.
 *
 * 2. Direct analyzer : set via set_parameter("analyzer", shared_ptr<UniversalAnalyzer<...>>)
 *    paired with set_parameter("analyzer_qualifier", std::string).
 *    Any existing concrete analyzer can be supplied pre-configured.
 *
 * 3. Analysis type : set via set_parameter("analysis_type", AnalysisType) with
 *    optional set_parameter("analyzer_qualifier", std::string). AttributeOp
 *    constructs a default-configured analyzer internally.
 *
 * Parameters common to all paths:
 * - feature_key  std::string  attribute name written on each grain  (default "feature")
 * - channel      uint32_t     source channel index                  (default 0)
 *
 * Qualifier strings map directly to scalar fields of the analysis result struct.
 * The alias "rms" is accepted for FEATURE and resolves to "mean_energy".
 * The alias "mean" is accepted for STATISTICAL and resolves to "mean_stat".
 *
 * FEATURE qualifiers (EnergyAnalysis):
 * - "mean_energy"  (default, alias: "rms")
 * - "max_energy"
 * - "min_energy"
 * - "variance"
 * - "dynamic_range"
 * - "event_count"
 * - "window_count"
 *
 * STATISTICAL qualifiers (StatisticalAnalysis):
 * - "mean_stat"  (default, alias: "mean")
 * - "max_stat"
 * - "min_stat"
 * - "variance"
 * - "std_dev"
 * - "skewness"
 * - "kurtosis"
 * - "median"
 * - "window_count"
 */
class MAYAFLUX_API AttributeOp final
    : public RegionGroupAnalyzer<Kakshya::RegionGroup> {
public:
    [[nodiscard]] AnalysisType get_analysis_type() const override
    {
        return AnalysisType::FEATURE;
    }

protected:
    [[nodiscard]] std::string get_analyzer_name() const override
    {
        return "Granular::AttributeOp";
    }

    Datum<Kakshya::RegionGroup> analyze_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;

private:
    /**
     * @brief Resolve and execute attribution for a single grain.
     * @param grain Source region defining the grain boundaries.
     * @param container Signal data source.
     * @param channel Channel index to extract samples from.
     * @param ctx Current execution context carrying parameters.
     * @return Scalar attribute value for this grain.
     */
    double compute_grain_attribute(
        const Kakshya::Region& grain,
        const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
        uint32_t channel,
        const ExecutionContext& ctx) const;

    /**
     * @brief Construct a default-configured analyzer for the given AnalysisType.
     * @param type Analysis category.
     * @return Heap-allocated analyzer with sensible defaults.
     */
    static std::shared_ptr<RegionGroupAnalyzer<Kakshya::RegionGroup>>
    make_default_analyzer(AnalysisType type);

    /**
     * @brief Extract a named scalar from a typed analysis result.
     * @param analysis_result std::any holding the concrete analysis struct.
     * @param type Analysis category used to select the correct cast path.
     * @param qualifier Names the specific scalar to extract.
     * @return Extracted double value.
     */
    static double extract_scalar(const std::any& analysis_result,
        AnalysisType type,
        const std::string& qualifier);

    mutable VariantEnergyAnalyzer<> m_energy_analyzer;
    mutable VariantStatisticalAnalyzer<> m_stat_analyzer;
};

/**
 * @class SortOp
 * @brief Sorts grains by a named attribute key, with optional GPU dispatch.
 *
 * Parameters (via set_parameter or ExecutionContext):
 * - feature_key         std::string  attribute to sort on
 * - ascending           bool         sort direction                    (default true)
 * - gpu_sort_threshold  uint32_t     grain count threshold for GPU path (default 0 = CPU only)
 */
class MAYAFLUX_API SortOp final
    : public StandardSorter<Kakshya::RegionGroup, Kakshya::RegionGroup> {
public:
    [[nodiscard]] SortingType get_sorting_type() const override
    {
        return SortingType::ALGORITHMIC;
    }

protected:
    [[nodiscard]] std::string get_sorter_name() const override
    {
        return "Granular::SortOp";
    }

    Datum<Kakshya::RegionGroup> sort_implementation(
        const Datum<Kakshya::RegionGroup>& input) override;
};

// ============================================================================
// Grammar rule executors
// ============================================================================

/// @brief Grammar rule executor for the segmentation step.
extern const ComputationGrammar::Rule::Executor segment_grains;

/// @brief Grammar rule executor for the attribution step.
extern const ComputationGrammar::Rule::Executor attribute_grains;

/// @brief Grammar rule executor for the sort step.
extern const ComputationGrammar::Rule::Executor sort_grains;

// ============================================================================
// Context construction
// ============================================================================

/**
 * @brief Construct an ExecutionContext for the granular pipeline using a built-in AnalysisType.
 *
 * AttributeOp will internally construct a default-configured analyzer for the
 * given type and extract the scalar named by qualifier. Omit qualifier to use
 * the canonical default for that type.
 *
 * @param grain_size        Samples per grain.
 * @param hop_size          Hop size between grain onsets.
 * @param feature_key       Attribute name written onto each grain Region.
 * @param analysis_type     Which analysis category to use for attribution. Supported: FEATURE, STATISTICAL.
 * @param qualifier         Scalar to extract from the analysis result. Empty uses type default.
 * @param channel           Source channel index.
 * @param ascending         Sort direction for the subsequent SortOp.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return Populated ExecutionContext.
 */
[[nodiscard]] inline ExecutionContext make_granular_context(
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AnalysisType analysis_type,
    const std::string& qualifier = {},
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0)
{
    ExecutionContext ctx;
    ctx.execution_metadata["grain_size"] = grain_size;
    ctx.execution_metadata["hop_size"] = hop_size;
    ctx.execution_metadata["channel"] = channel;
    ctx.execution_metadata["feature_key"] = feature_key;
    ctx.execution_metadata["ascending"] = ascending;
    ctx.execution_metadata["gpu_sort_threshold"] = gpu_sort_threshold;
    ctx.execution_metadata["analysis_type"] = analysis_type;
    ctx.execution_metadata["analyzer_qualifier"] = qualifier;
    return ctx;
}

/**
 * @brief Construct an ExecutionContext supplying a pre-configured analyzer instance directly.
 *
 * Use when the default analyzer configuration is insufficient and you need
 * specific window sizes, methods, or other parameters set on the analyzer beforehand.
 *
 * @tparam InputType  Analyzer input data type.
 * @tparam OutputType Analyzer output data type.
 * @param grain_size        Samples per grain.
 * @param hop_size          Hop size between grain onsets.
 * @param feature_key       Attribute name written onto each grain Region.
 * @param analyzer          Pre-configured analyzer instance.
 * @param qualifier         Scalar to extract from the analysis result. Empty uses type default.
 * @param channel           Source channel index.
 * @param ascending         Sort direction for the subsequent SortOp.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return Populated ExecutionContext.
 */
template <ComputeData InputType, ComputeData OutputType>
[[nodiscard]] inline ExecutionContext make_granular_context(
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    std::shared_ptr<UniversalAnalyzer<InputType, OutputType>> analyzer,
    const std::string& qualifier = {},
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0)
{
    ExecutionContext ctx;
    ctx.execution_metadata["grain_size"] = grain_size;
    ctx.execution_metadata["hop_size"] = hop_size;
    ctx.execution_metadata["channel"] = channel;
    ctx.execution_metadata["feature_key"] = feature_key;
    ctx.execution_metadata["ascending"] = ascending;
    ctx.execution_metadata["gpu_sort_threshold"] = gpu_sort_threshold;
    ctx.execution_metadata["analyzer"] = std::static_pointer_cast<void>(analyzer);
    ctx.execution_metadata["analyzer_qualifier"] = qualifier;
    return ctx;
}

/**
 * @brief Construct an ExecutionContext using a span-level lambda for fully custom attribution.
 *
 * Use when the feature computation cannot be expressed through any existing analyzer.
 *
 * @param grain_size        Samples per grain.
 * @param hop_size          Hop size between grain onsets.
 * @param feature_key       Attribute name written onto each grain Region.
 * @param executor          Lambda receiving grain samples and context, returning a scalar.
 * @param channel           Source channel index.
 * @param ascending         Sort direction for the subsequent SortOp.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return Populated ExecutionContext.
 */
[[nodiscard]] inline ExecutionContext make_granular_context(
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AttributeExecutor executor,
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0)
{
    ExecutionContext ctx;
    ctx.execution_metadata["grain_size"] = grain_size;
    ctx.execution_metadata["hop_size"] = hop_size;
    ctx.execution_metadata["channel"] = channel;
    ctx.execution_metadata["feature_key"] = feature_key;
    ctx.execution_metadata["ascending"] = ascending;
    ctx.execution_metadata["gpu_sort_threshold"] = gpu_sort_threshold;
    ctx.execution_metadata["attribute_executor"] = std::move(executor);
    return ctx;
}

// ============================================================================
// Input construction
// ============================================================================

/**
 * @brief Construct the initial GranularDatum from a container.
 * @param container Source signal data.
 * @param group_name Name assigned to the RegionGroup. Defaults to "grains".
 * @return GranularDatum ready for pipeline input.
 */
[[nodiscard]] inline GranularDatum make_granular_input(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    const std::string& group_name = "grains")
{
    return GranularDatum { Kakshya::RegionGroup { group_name }, container };
}

// ============================================================================
// Matrix factory
// ============================================================================

/**
 * @brief Construct a GranularMatrix with grammar rules appropriate for the
 *        requested output type.
 *
 * REGION_GROUP: registers segment, attribute, sort (existing behaviour).
 * CONTAINER: registers segment, attribute, sort, then appends reconstruct.
 *            The reconstruct rule reads the source container from
 *            ExecutionContext::execution_metadata["container"] and writes
 *            stitched per-channel audio into a SoundFileContainer.
 * CONTAINER_ADDITIVE: same as CONTAINER but uses an overlap-add reconstruct rule
 *
 * @param attribution_context ComputationContext assigned to the attribution rule.
 * @param output              Terminal output type for this pipeline.
 * @param taper               Optional per-grain taper for OLA output.
 * @return Configured GranularMatrix instance.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<GranularMatrix> make_granular_matrix(
    ComputationContext attribution_context = ComputationContext::SPECTRAL,
    GranularOutput output = GranularOutput::REGION_GROUP, GrainTaper taper = {});

// ============================================================================
// Free process function
// ============================================================================

/**
 * @brief Zero-config entry point for the full granular pipeline using a built-in AnalysisType.
 *
 * Constructs context and matrix internally, runs segment -> attribute -> sort,
 * and returns the attributed, sorted RegionGroup.
 *
 * @param container         Source signal data.
 * @param grain_size        Samples per grain.
 * @param hop_size          Hop size between grain onsets.
 * @param feature_key       Attribute name written onto each grain Region.
 * @param analysis_type     Which analysis category to use for attribution.
 * @param qualifier         Scalar to extract from the analysis result. Empty uses type default.
 * @param channel           Source channel index.
 * @param ascending         Sort direction.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return GranularDatum containing the attributed, sorted RegionGroup.
 */
[[nodiscard]] MAYAFLUX_API GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AnalysisType analysis_type,
    const std::string& qualifier = {},
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0);

/**
 * @brief Zero-config entry point for the full granular pipeline using a span-level lambda.
 *
 * @param container         Source signal data.
 * @param grain_size        Samples per grain.
 * @param hop_size          Hop size between grain onsets.
 * @param feature_key       Attribute name written onto each grain Region.
 * @param executor          Lambda receiving grain samples and context, returning a scalar.
 * @param channel           Source channel index.
 * @param ascending         Sort direction.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return GranularDatum containing the attributed, sorted RegionGroup.
 */
[[nodiscard]] MAYAFLUX_API GranularDatum process(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AttributeExecutor executor,
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0);

/**
 * @brief Offline granular workflow: segment, attribute, sort, reconstruct.
 *
 * Runs the full segment → attribute → sort pipeline on @p container, then
 * stitches the sorted grains per channel into a SoundFileContainer ready
 * for the standard ContiguousAccessProcessor playback path.
 *
 * @param container          Source signal data.
 * @param grain_size         Samples per grain.
 * @param hop_size           Hop size between grain onsets.
 * @param feature_key        Attribute name written onto each grain Region.
 * @param analysis_type      Analysis category used for attribution.
 * @param qualifier          Scalar to extract from the analysis result. Empty uses type default.
 * @param channel            Source channel index.
 * @param ascending          Sort direction.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return Populated SoundFileContainer containing reconstructed audio in sorted grain order.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AnalysisType analysis_type,
    const std::string& qualifier = {},
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0,
    ComputationContext attribution_context = ComputationContext::SPECTRAL);

/**
 * @brief Offline granular workflow using a span-level attribution lambda.
 *
 * @param container          Source signal data.
 * @param grain_size         Samples per grain.
 * @param hop_size           Hop size between grain onsets.
 * @param feature_key        Attribute name written onto each grain Region.
 * @param executor           Lambda receiving grain samples and context, returning a scalar.
 * @param channel            Source channel index.
 * @param ascending          Sort direction.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path. 0 = CPU only.
 * @return Populated SoundFileContainer containing reconstructed audio in sorted grain order.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SoundFileContainer> process_to_container(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AttributeExecutor executor,
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0,
    ComputationContext attribution_context = ComputationContext::SPECTRAL);

/**
 * @brief Offline granular workflow with OLA reconstruction.
 *
 * Runs segment → attribute → sort → overlap-add reconstruct.
 * Grains are accumulated at hop_size intervals; @p taper is applied
 * to each grain before accumulation.
 *
 * @param container          Source signal data.
 * @param grain_size         Samples per grain.
 * @param hop_size           Hop size between grain onsets.
 * @param feature_key        Attribute name written onto each grain Region.
 * @param analysis_type      Analysis category used for attribution.
 * @param qualifier          Scalar to extract. Empty uses type default.
 * @param channel            Source channel index.
 * @param ascending          Sort direction.
 * @param gpu_sort_threshold Grain count at which SortOp switches to GPU path.
 * @param attribution_context ComputationContext for the attribution rule.
 * @param taper              Optional per-grain taper. Pass {} for none.
 * @return Populated SoundFileContainer with overlap-added audio.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SoundFileContainer> process_to_container_additive(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& container,
    uint32_t grain_size,
    uint32_t hop_size,
    const std::string& feature_key,
    AnalysisType analysis_type,
    const std::string& qualifier = {},
    uint32_t channel = 0,
    bool ascending = true,
    uint32_t gpu_sort_threshold = 0,
    ComputationContext attribution_context = ComputationContext::SPECTRAL,
    GrainTaper taper = {});

} // namespace MayaFlux::Yantra::Granular
