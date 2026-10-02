#pragma once

#include "MayaFlux/Kinesis/Tendency/Tendency.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {
class TextureArrayBuffer;
}

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Kakshya {
class DynamicVideoStream;
}

namespace MayaFlux::Vruta {
class Routine;
}

namespace MayaFlux::Kriya {

class BufferPipeline;
class BufferOperation;

/**
 * @class Chimera
 * @brief Assembles the layers of a TextureArrayBuffer from whatever feeds them, so a
 *        fragment shader can build one picture out of many.
 *
 * A layer holds a still image, a GPU image copied every frame, or a moment of a ring
 * that is still being recorded. Moments are read at a lag behind the write head, with
 * a lag that varies in time, or played freely from where the layer entered, forward,
 * held or backward, optionally smoothed between frames. A layer can be cut to
 * another moment, or given a new source, while running. Layers are numbered in
 * declaration order, enter after their own delay, and refresh every frame or every
 * few. Copies share the same layers, and the set stops when the last copy goes.
 *
 * A lagged layer stays at least two frames behind the frame being written. A layer
 * whose position falls outside what the ring still holds keeps its last frame and
 * reports once. stop() holds every layer on its last frame.
 *
 * Built from a buffer with ChimeraBuilder or create_chimera. The layers are fed by
 * the cycles of the builder's pipeline, one per frame, after whatever the pipeline
 * records. What the layers mean on screen is the buffer's fragment shader. Call the
 * controls from the frame clock.
 */
class MAYAFLUX_API Chimera {
public:
    Chimera() = default;

    /** @brief Number of layers. */
    [[nodiscard]] size_t layer_count() const;

    /**
     * @brief Set the weight of one layer in the buffer's push constants.
     * @param layer  Layer index.
     * @param weight What the shader does with it; zero or less hides the layer in
     *               the shipped shader.
     */
    void level(size_t layer, double weight);

    /**
     * @brief Set the params of one layer in the buffer's per-layer data.
     *
     * Placement, scale, a mask or anything else the shader reads from
     * layerData[2 * layer]. Enables the buffer's layer data.
     */
    void params(size_t layer, const glm::vec4& values);

    /**
     * @brief Change the speed of a freely playing layer.
     *
     * Applies from the current position, so it never jumps. Zero holds, a negative
     * ratio plays backward. A lagged layer ignores it.
     *
     * @param layer Layer index.
     * @param ratio Multiple of the ring's frame rate.
     */
    void speed(size_t layer, double ratio);

    /**
     * @brief Jump a ring layer to a moment this many seconds behind the live head.
     *
     * A lagged layer keeps following its lag from there, so a lag that varies carries
     * on from the new moment. Layers fed by an image ignore it.
     *
     * @param layer   Layer index.
     * @param seconds Distance behind the write head, at least two frames.
     */
    void cut(size_t layer, double seconds);

    /** @brief Replace the layer's content with a still image, delivered on the next frame. */
    void set(size_t layer, Kakshya::ImageData image);

    /** @brief Replace the layer's content with a GPU image copied every frame. */
    void set(size_t layer, std::shared_ptr<Core::VKImage> image);

    /** @brief Replace the layer's content with a ring, read as the layer's lag or speed say. */
    void set(size_t layer, std::shared_ptr<Kakshya::DynamicVideoStream> ring);

    /** @brief Stop feeding. Every layer keeps its last frame. */
    void stop();

private:
    friend class ChimeraBuilder;

    struct Layer {
        std::shared_ptr<Kakshya::DynamicVideoStream> ring;
        std::shared_ptr<Core::VKImage> image;
        std::shared_ptr<const Kakshya::ImageData> picture;
        std::shared_ptr<const Kinesis::TimeMap> lag;
        std::shared_ptr<double> ratio { std::make_shared<double>(1.0) };
        std::optional<Portal::Graphics::FitMode> fit;
        std::optional<glm::vec4> params;
        std::vector<uint8_t> scratch;
        double delay {};
        double lag_offset {};
        double position {};
        float level { 1.0F };
        uint32_t interval { 1 };
        bool smooth {};
        bool entered {};
        bool missed {};
        bool dirty {};
        bool piped {};
    };

    struct State {
        std::shared_ptr<Buffers::TextureArrayBuffer> buffer;
        std::shared_ptr<BufferPipeline> pipeline;
        std::vector<Layer> layers;
        double frame_rate { 60.0 };
        uint64_t ticks {};

        void halt();
    };

    static void tick(State& state, uint64_t frame);
    static void feed(State& state, Layer& layer, uint32_t index, double elapsed);
    static void stamp(State& state, uint32_t index, const glm::vec4& timing);

    std::shared_ptr<State> m_state;
    std::shared_ptr<void> m_guard;
};

/**
 * @class ChimeraBuilder
 * @brief Declares the layers of a Chimera, in order, and starts feeding them.
 *
 * layer() opens a new layer and the calls after it describe that layer. A call made
 * before any layer() describes the first.
 *
 * @code{.cpp}
 * auto chimera = MayaFlux::create_chimera(array)
 *                    .layer().from("res/eye.png").params({ 0.3F, 0.4F, 0.2F, 0.F })
 *                    .layer().from("res/mouth.png").params({ 0.5F, 0.7F, 0.3F, 0.F })
 *                    .layer().from(ring).lag(0.0)
 *                    .layer().from(ring).lag(0.5).smooth().level(0.6)
 *                    .start();
 * @endcode
 *
 * The builder holds one BufferPipeline, the one it was given or one it was made
 * with, so what is recorded and what is assembled read as one chain:
 *
 * @code{.cpp}
 * auto chimera = MayaFlux::create_chimera(array)
 *                    .record(BufferOperation::capture_to_stream(io, camera, 240))
 *                    .layer().from_pipeline().lag(0.0)
 *                    .layer().from_pipeline().lag(0.5)
 *                    .start();
 * @endcode
 */
class MAYAFLUX_API ChimeraBuilder {
public:
    /**
     * @param buffer   Array buffer whose layers are fed.
     * @param pipeline The pipeline that records, and whose cycles feed the layers.
     */
    ChimeraBuilder(
        std::shared_ptr<Buffers::TextureArrayBuffer> buffer,
        std::shared_ptr<BufferPipeline> pipeline);

    /** @brief The pipeline the builder records into. */
    [[nodiscard]] std::shared_ptr<BufferPipeline> get_pipeline() const { return m_pipeline; }

    /**
     * @brief Replace the builder's pipeline with one made elsewhere.
     *
     * The pipeline must not be running. start() adds the feeding to the end of its
     * operations and runs it at the frame rate, one cycle per frame, and stop() ends
     * it. Operations the builder recorded earlier stay in the pipeline it had.
     */
    ChimeraBuilder& use_pipeline(std::shared_ptr<BufferPipeline> pipeline);

    /** @brief Add an operation to the builder's pipeline, ahead of the feeding. */
    ChimeraBuilder& record(BufferOperation&& operation);

    /**
     * @brief Call an action on the running Chimera every this many seconds.
     *
     * Runs in the pipeline after the layers are fed, on the frame clock, first one
     * interval after start(). The action may call any control, such as cut, set,
     * speed or level. It does not belong to a layer.
     *
     * @param seconds Interval, at least one frame.
     * @param action  Called with the running Chimera.
     */
    ChimeraBuilder& every(double seconds, std::function<void(Chimera&)> action);

    /**
     * @brief Draw the buffer this way once start() runs.
     *
     * Rendering is set up after every layer's params and the layer data are in place,
     * so a shader that reads layerData is bound correctly. Leave it out for a buffer
     * that already renders.
     */
    ChimeraBuilder& render(Portal::Graphics::RenderConfig config);

    /**
     * @brief Create the per-layer buffer the shader reads as layerData.
     *
     * For a shader that reads the timing a Chimera writes and declares the buffer.
     * Layers given params enable it without this call.
     */
    ChimeraBuilder& layer_data();

    /** @brief Open the next layer. */
    ChimeraBuilder& layer();

    /** @brief Feed the layer from the stream the pipeline records, read when start() runs. */
    ChimeraBuilder& from_pipeline();

    /** @brief Feed the layer from a ring that is being recorded. */
    ChimeraBuilder& from(std::shared_ptr<Kakshya::DynamicVideoStream> ring);

    /** @brief Feed the layer by copying a GPU image every frame. */
    ChimeraBuilder& from(std::shared_ptr<Core::VKImage> image);

    /** @brief Fill the layer once with a still image. */
    ChimeraBuilder& from(Kakshya::ImageData image);

    /** @brief Fill the layer once with an image file, loaded now as RGBA. */
    ChimeraBuilder& from(const std::string& path);

    /** @brief Seconds after start() before the layer begins to be fed. */
    ChimeraBuilder& enters_after(double seconds);

    /** @brief Read the ring this many seconds behind its write head, at least two frames. */
    ChimeraBuilder& lag(double seconds);

    /** @brief Read the ring behind its write head by a lag that varies with time, in seconds since the layer entered. */
    ChimeraBuilder& lag(Kinesis::TimeMap seconds);

    /** @brief Play a layer without a lag at this multiple of the ring's frame rate. Negative plays backward. */
    ChimeraBuilder& speed(double ratio);

    /** @brief Blend the two frames around a ring position, so slow or varying time does not step. */
    ChimeraBuilder& smooth(bool enable = true);

    /** @brief Refresh a ring or GPU image layer every this many frames. */
    ChimeraBuilder& every_n_frames(uint32_t frames);

    /** @brief Fit frames of another extent into this layer in this way, instead of the buffer's. */
    ChimeraBuilder& fit(Portal::Graphics::FitMode mode);

    /** @brief Params for the layer in the buffer's per-layer data, such as placement. Enables it. */
    ChimeraBuilder& params(const glm::vec4& values);

    /** @brief Weight passed to the shader for the layer. */
    ChimeraBuilder& level(double weight);

    /**
     * @brief Start feeding every declared layer on the frame clock.
     * @return The running set, or an empty one if the buffer is null, no layer was
     *         declared, or more layers were declared than the buffer holds.
     */
    [[nodiscard]] Chimera start();

private:
    struct Action {
        double seconds {};
        std::function<void(Chimera&)> run;
    };

    Chimera::Layer& current();

    std::shared_ptr<Buffers::TextureArrayBuffer> m_buffer;
    std::shared_ptr<BufferPipeline> m_pipeline;
    std::optional<Portal::Graphics::RenderConfig> m_render;
    std::vector<Chimera::Layer> m_layers;
    std::vector<Action> m_actions;
};

} // namespace MayaFlux::Kriya
