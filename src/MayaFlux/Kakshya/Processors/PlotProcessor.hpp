#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Nodes {
class Node;
namespace Network {
    class NodeNetwork;
}
}

namespace MayaFlux::Buffers {
class AudioBuffer;
}

namespace MayaFlux::Kakshya {

/**
 * @class PlotProcessor
 * @brief DataProcessor that acquires per-series data from heterogeneous sources
 *        and writes into PlotContainer::m_data each process() call.
 *
 * Mirrors the source-binding model of DescriptorBindingsProcessor but targets
 * CPU-side DataVariant storage rather than GPU descriptor sets. Each series slot
 * in the container has one binding. All reads are EXTERNAL — the processor never
 * drives node or network processing; it only samples already-processed state.
 *
 * Source types per series:
 *   NODE         — single Node, reads get_last_output() → scalar appended or
 *                  overwrites the series depending on mode (rolling vs snapshot)
 *   AUDIO_BUFFER — AudioBuffer, reads get_data() span → full series copy
 *   NETWORK      — NodeNetwork with audio output, reads get_audio_buffer() → full series copy
 *   CALLABLE     — std::function<void(std::vector<double>&)>, caller fills the series
 *   RAW          — pending std::vector<double> pushed via set_raw(), swapped in on process()
 *
 * process() iterates all bindings, acquires from source, writes into the container's
 * m_data variant at the bound series index, then copies m_data into processed_data.
 * Called by Forma immediately before the geometry function runs.
 */
class MAYAFLUX_API PlotProcessor : public DataProcessor {
public:
    enum class SourceType : uint8_t {
        NODE,
        AUDIO_BUFFER,
        NETWORK,
        CALLABLE,
        RAW,
    };

    struct SeriesBinding {
        SourceType source_type { SourceType::RAW };
        DataDimension::Role role { DataDimension::Role::CUSTOM };
        DataModality modality { DataModality::TENSOR_ND };

        std::shared_ptr<Nodes::Node> node;
        std::shared_ptr<Buffers::AudioBuffer> audio_buffer;
        std::shared_ptr<Nodes::Network::NodeNetwork> network;
        std::function<void(std::vector<double>&)> callable;

        std::vector<double> pending_raw;
        std::atomic_flag raw_dirty = ATOMIC_FLAG_INIT;
    };

    PlotProcessor() = default;
    ~PlotProcessor() override = default;

    // =========================================================================
    // Binding
    // =========================================================================

    /**
     * @brief Bind a series slot to a Node.
     *
     * Each process() calls Buffers::extract_multiple_samples(node, series_size),
     * which handles snapshot context, save/restore state, and buffer lifecycle
     * identically to NodeSourceProcessor. The series is filled with one full
     * batch of node output per process() call.
     *
     * @param series_index  Index returned by PlotContainer::add_series().
     * @param node          Node to read from.
     */
    void bind_node(uint32_t series_index,
        std::shared_ptr<Nodes::Node> node);

    /**
     * @brief Bind a series slot to an AudioBuffer.
     *
     * Each process() copies the full buffer span into the series.
     * Series size is resized to match the buffer on the first process() call.
     *
     * @param series_index  Index returned by PlotContainer::add_series().
     * @param buffer        AudioBuffer to read from.
     */
    void bind_audio_buffer(uint32_t series_index,
        std::shared_ptr<Buffers::AudioBuffer> buffer);

    /**
     * @brief Bind a series slot to a NodeNetwork with audio output.
     *
     * Each process() reads get_audio_buffer() and copies the result into the series.
     * Fails at bind time if the network has no audio output mode.
     *
     * @param series_index  Index returned by PlotContainer::add_series().
     * @param network       NodeNetwork to read from.
     */
    void bind_network(uint32_t series_index,
        std::shared_ptr<Nodes::Network::NodeNetwork> network);

    /**
     * @brief Bind a series slot to a callable.
     *
     * Each process() calls fn with the series vector by reference. The callable
     * fills or mutates it freely. Suitable for computed series, ring buffer views,
     * and any source that does not fit the other patterns.
     *
     * @param series_index  Index returned by PlotContainer::add_series().
     * @param fn            Callable invoked with the series vector each process().
     */
    void bind_callable(uint32_t series_index,
        std::function<void(std::vector<double>&)> fn);

    /**
     * @brief Push raw sample data for a series.
     *
     * Lock-free pending swap. The data is committed into the container on the
     * next process() call. Thread-safe: may be called from any thread.
     *
     * @param series_index  Index returned by PlotContainer::add_series().
     * @param data          Samples to write. Copied into a pending buffer.
     */
    void set_raw(uint32_t series_index, std::vector<double> data);

    /**
     * @brief Remove a binding from a series slot.
     * @param series_index Series index to unbind.
     */
    void unbind(uint32_t series_index);

    /**
     * @brief Set the role and modality for a series binding.
     *
     * Called by PlotContainer after every bind_* to propagate the semantic
     * metadata declared on add_series() down into the binding. process() uses
     * these values to tag each processed_data variant's dimension correctly,
     * so DomainMapping can query by Role rather than by index.
     *
     * @param series_index  Series index.
     * @param role          DataDimension::Role declared on add_series().
     * @param modality      DataModality declared on add_series().
     */
    void set_series_semantics(uint32_t series_index,
        DataDimension::Role role,
        DataModality modality);

    [[nodiscard]] bool has_binding(uint32_t series_index) const;

    // =========================================================================
    // DataProcessor
    // =========================================================================

    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Acquire data from all bound sources and write into the container.
     *
     * For each bound series: acquires from source, writes into container m_data,
     * then copies m_data into processed_data. Unbound series are left unchanged.
     * Called from the graphics thread by Forma before the geometry function runs.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    [[nodiscard]] bool is_processing() const override
    {
        return m_processing.load(std::memory_order_acquire);
    }

private:
    void acquire_from_node(SeriesBinding& b, std::vector<double>& series);
    void acquire_from_audio_buffer(SeriesBinding& b, std::vector<double>& series);
    void acquire_from_network(SeriesBinding& b, std::vector<double>& series);
    void acquire_from_callable(SeriesBinding& b, std::vector<double>& series);
    void acquire_from_raw(SeriesBinding& b, std::vector<double>& series);

    std::unordered_map<uint32_t, SeriesBinding> m_bindings;
    std::atomic<bool> m_processing { false };
};

} // namespace MayaFlux::Kakshya
