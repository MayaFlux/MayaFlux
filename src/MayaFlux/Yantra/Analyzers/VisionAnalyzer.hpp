#pragma once

#include "MayaFlux/Kinesis/Vision/VisionInsight.hpp"
#include "MayaFlux/Kinesis/Vision/VisionQuery.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/TextureExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/VisionGpuDispatch.hpp"
#include "MayaFlux/Yantra/FunctionalOperation.hpp"

/**
 * @file VisionAnalyzer.hpp
 * @brief FunctionalOperation<shared_ptr<SignalSourceContainer>,
 *        vector<DataVariant>> whose real API matches EnergyAnalyzer/
 *        StatisticalAnalyzer's own convention: query/context are
 *        constructor arguments and setters, not call arguments, and
 *        analyze_vision(source) takes only the data, the same shape as
 *        analyze_energy(data)/analyze_statistics(data).
 *
 * width/height are never passed in: VKImage and every container Vision
 * accepts (TextureContainer, VideoStreamContainer, WindowContainer) already
 * report their own get_width()/get_height().
 */

namespace MayaFlux::Yantra::Vision {
class VisionMatrix;
}

namespace MayaFlux::Kakshya {
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @class VisionAnalyzer
 * @brief Resolves its configured VisionQuery, runs every resolved
 *        VisionSequence against a persistent VisionGpuExecutor, then runs
 *        the GPU reductions (track_reduce.comp) over the (context-filtered)
 *        subject tracks/keypoints for centroid/velocity/bounds.
 *
 * Owns its VisionGpuExecutor for the same reason VisionProcessor does:
 * cross-frame state (retained optical flow atlases and detections) lives
 * on the executor's contexts and must survive between calls.
 */
class MAYAFLUX_API VisionAnalyzer
    : public FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>> {
public:
    using Base = FunctionalOperation<std::shared_ptr<Kakshya::SignalSourceContainer>, std::vector<Kakshya::DataVariant>>;

    /**
     * @brief Construct with the intents to satisfy and the caller-supplied
     *        filtering context, matching EnergyAnalyzer's
     *        constructor-configures-the-run convention.
     */
    explicit VisionAnalyzer(
        Kinesis::Vision::VisionQuery query = {},
        Kinesis::Vision::VisionAnalysisContext context = {});

    VisionAnalyzer(const VisionAnalyzer&) = delete;
    VisionAnalyzer& operator=(const VisionAnalyzer&) = delete;

    void set_query(const Kinesis::Vision::VisionQuery& query) { m_query = query; }
    [[nodiscard]] const Kinesis::Vision::VisionQuery& get_query() const { return m_query; }

    void set_context(const Kinesis::Vision::VisionAnalysisContext& context) { m_context = context; }
    [[nodiscard]] const Kinesis::Vision::VisionAnalysisContext& get_context() const { return m_context; }

    /**
     * @brief Real entry point: resolve the configured query against
     *        source, run it, run the GPU analysis. Same shape as
     *        analyze_energy(data)/analyze_statistics(data): only the data
     *        is a call argument. To change what a later call does, call
     *        set_query()/set_context() first, the same way a caller
     *        changes EnergyAnalyzer's behaviour with set_method() before
     *        the next analyze_energy(), not by passing it alongside data.
     *
     * @param source Image-bearing container (VideoStreamContainer,
     *               WindowContainer, or TextureContainer; the same set
     *               VisionProcessor accepts). Anything else yields an
     *               empty VisionAnalysis.
     * @param index  Layer/frame index, used only for a TextureContainer's
     *               to_image(index); ignored for the other two, which have
     *               no multi-index concept and always give their current
     *               frame.
     */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze_vision(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        size_t index = 0);

    /**
     * @brief analyze_vision(), for a caller that already has a resolved
     *        GPU image (e.g. VisionExtractor's own output) rather than a
     *        container. Skips container resolution entirely; otherwise
     *        identical.
     */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze_vision(
        const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief Advance an analysis of @p image by one sequence per call,
     *        without waiting on deferred GPU work.
     *
     * The first call copies @p image and starts a run on the copy. Later
     * calls continue that run and ignore @p image until it completes. The
     * flow steps of TrackObjects and EstimateMotion are submitted deferred
     * and polled on later calls; other steps still wait inline.
     *
     * @return The analysis once every sequence has completed, otherwise
     *         nullopt.
     */
    [[nodiscard]] std::optional<Kinesis::Vision::VisionAnalysis> advance(
        const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief The copy of the frame the current or last advance() run
     *        analyzes. Replaced when the next run starts.
     */
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& get_frame() const { return m_frame; }

    /**
     * @brief The VisionResult of every sequence of the last completed
     *        analysis, in the order they ran. Empty before the first.
     *
     * The images in each result are named by the op that produced them and
     * belong to that sequence's executor: they are valid until the next
     * analysis runs. A sequence that failed leaves an empty result.
     */
    [[nodiscard]] const std::vector<Kinesis::Vision::VisionResult>& get_results() const { return *m_results; }

    /**
     * @brief The same results, owned: they stay valid after later analyses,
     *        which replace them with new ones.
     *
     * The images are the executors' own and are rewritten in place when
     * their sequence runs again.
     */
    [[nodiscard]] std::shared_ptr<const std::vector<Kinesis::Vision::VisionResult>> get_shared_results() const { return m_results; }

    /**
     * @brief Abandon outstanding work and clear retained executor state.
     *
     * Call when the pixel source changes (camera switch, video seek).
     */
    void reset();

    /**
     * @brief The executor that ran the FindElements sequence, or the first
     *        executor when the query has none.
     *
     * Each sequence of a query runs on its own executor, so the images one
     * sequence leaves in its VisionResult are not overwritten by the next.
     * VisionExtractor::mask() needs the executor holding the label buffer
     * that VisionAnalysis::find_elements indexes into, which is this one.
     *
     * Valid after at least one analyze_vision() call; null before that.
     */
    [[nodiscard]] VisionGpuExecutor* get_executor() const;

private:
    friend class Vision::VisionMatrix;

    /**
     * @brief ComputeOperation adapter: uses input.data as the source
     *        container, pulls index out of Datum metadata (defaulting to
     *        0), calls analyze_vision(). See analyze_vision() for the real
     *        API; query/context come from this instance's own
     *        configured state, not from the Datum, matching every other
     *        UniversalAnalyzer-family operation_function.
     */
    output_type run_operation(const input_type& input);

    /**
     * @brief Resolve source/index to a GPU image, the same way
     *        VisionProcessor does: TextureContainer's own to_image(index)
     *        (zero-copy cache), or get_raw_data() uploaded into a
     *        persistent m_upload_image for VideoStreamContainer/
     *        WindowContainer. Null on an unsupported container type.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> resolve_image(
        const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
        size_t index);

    /**
     * @brief Shared body of both analyze_vision() overloads, once a GPU
     *        image is in hand: resolve the configured query, run every
     *        resolved VisionSequence, apply context filtering, run the
     *        GPU reductions. w/h are the image's own dimensions.
     */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis analyze_resolved(
        const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief Run or poll sequence @p lane of @p resolved on its executor.
     * @return False while the sequence is suspended on deferred work; true
     *         once its result is appended to @p results.
     */
    bool run_lane(
        size_t lane,
        const std::vector<Kinesis::Vision::ResolvedSequence>& resolved,
        std::vector<Kinesis::Vision::VisionResult>& results,
        const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief Route every completed sequence result into one VisionAnalysis,
     *        apply context filtering and run the GPU reductions.
     */
    [[nodiscard]] Kinesis::Vision::VisionAnalysis assemble(
        const std::vector<Kinesis::Vision::ResolvedSequence>& resolved,
        const std::vector<Kinesis::Vision::VisionResult>& results,
        uint32_t w, uint32_t h);

    /** @brief Copy @p image into this analyzer's own frame image. Null on failure. */
    [[nodiscard]] std::shared_ptr<Core::VKImage> hold_frame(const std::shared_ptr<Core::VKImage>& image);

    /**
     * @brief State of the run advance() is stepping through.
     *
     * resolved is fixed for the whole run: a suspended executor keeps a
     * pointer to the sequence it is walking.
     */
    struct StepRun {
        std::vector<Kinesis::Vision::ResolvedSequence> resolved;
        std::vector<Kinesis::Vision::VisionResult> results;
        uint32_t w {};
        uint32_t h {};
        bool active {};
    };

    /**
     * @brief Dispatch track_reduce.comp over tracks and fill
     *        centroid/velocity/bounds/mean_speed_px/age stats on the
     *        result. No-op (default fields) when tracks is empty.
     */
    [[nodiscard]] Kinesis::Vision::TrackObjectsAnalysis reduce_tracks(
        const std::vector<Kinesis::Vision::TrackResult>& tracks, uint32_t w, uint32_t h);

    /**
     * @brief Dispatch track_reduce.comp over keypoint positions (fed as
     *        their own previous, so velocity reduces to zero and is simply
     *        not surfaced) and fill centroid/bounds/mean_response on the
     *        result. No-op (default fields) when keypoints is empty. Same
     *        shader as reduce_tracks, not a duplicate: a keypoint has no
     *        persistent identity or previous position of its own, but its
     *        bounds and centroid are the same min/max/sum reduction either
     *        way.
     */
    [[nodiscard]] Kinesis::Vision::DetectFeaturesAnalysis reduce_keypoints(
        const std::vector<Kinesis::Vision::Keypoint>& keypoints);

    /**
     * @brief Shape descriptors (element_shape.comp) and nearest-neighbour
     *        distances (nearest_neighbor.comp) over fe.boxes, filled in
     *        place. No-op when fe.boxes is empty.
     */
    void compute_shapes(Kinesis::Vision::FindElementsAnalysis& fe, uint32_t w, uint32_t h);

    /**
     * @brief MeasureAppearance's whole-frame gradient sample (region_sample.comp
     *        over result.images.sobel) and brightness histogram
     *        (otsu_histogram.comp over gray). gray is the frame's gray image,
     *        which a sequence seeded from another lane's RgbaToGray does not
     *        produce itself, so the caller supplies it. Empty fields for
     *        whichever input image is null.
     */
    [[nodiscard]] Kinesis::Vision::MeasureAppearanceAnalysis measure_appearance(
        const Kinesis::Vision::VisionResult& result,
        const std::shared_ptr<Core::VKImage>& gray,
        uint32_t w, uint32_t h);

    /** @brief Dispatch region_sample.comp over the whole image (no region concept for MeasureAppearance). */
    [[nodiscard]] Kinesis::Vision::FieldSample sample_full_frame(
        const std::shared_ptr<Core::VKImage>& image, uint32_t w, uint32_t h);

    /** @brief Dispatch otsu_histogram.comp over image, zeroing the buffer first. */
    [[nodiscard]] std::array<uint32_t, 256> compute_histogram(
        const std::shared_ptr<Core::VKImage>& image, uint32_t w, uint32_t h);

    /** @brief Dispatch histogram_reduce.comp over an already-computed histogram. */
    [[nodiscard]] float compute_mean_brightness(const std::array<uint32_t, 256>& histogram);

    Kinesis::Vision::VisionQuery m_query;
    Kinesis::Vision::VisionAnalysisContext m_context;

    std::vector<std::unique_ptr<VisionGpuExecutor>> m_executors;
    size_t m_label_lane { 0 };
    std::shared_ptr<ShaderExecutionContext<>> m_track_reducer;
    std::shared_ptr<ShaderExecutionContext<>> m_brightness_reducer;
    std::shared_ptr<ShaderExecutionContext<>> m_shape_ctx;
    std::shared_ptr<ShaderExecutionContext<>> m_neighbor_ctx;
    std::shared_ptr<TextureExecutionContext> m_gradient_sample_ctx;
    std::shared_ptr<TextureExecutionContext> m_histogram_ctx;

    std::shared_ptr<Core::VKImage> m_upload_image;
    uint32_t m_upload_w { 0 };
    uint32_t m_upload_h { 0 };

    StepRun m_run;
    std::shared_ptr<const std::vector<Kinesis::Vision::VisionResult>> m_results {
        std::make_shared<const std::vector<Kinesis::Vision::VisionResult>>()
    };
    Portal::Graphics::ImageCacheEntry m_frame_cache;
    std::shared_ptr<Core::VKImage> m_frame;
};

} // namespace MayaFlux::Yantra
