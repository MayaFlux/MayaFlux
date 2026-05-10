#include "PlotProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferUtils.hpp"

#include "MayaFlux/Kakshya/Source/PlotContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Kakshya {

namespace {
    constexpr auto C = Journal::Component::Kakshya;
    constexpr auto X = Journal::Context::ContainerProcessing;
}

// =========================================================================
// Binding
// =========================================================================

void PlotProcessor::bind_node(uint32_t series_index,
    std::shared_ptr<Nodes::Node> node)
{
    if (!node) {
        MF_ERROR(C, X, "PlotProcessor::bind_node: null node for series {}", series_index);
        return;
    }
    auto& b = m_bindings[series_index];
    b.source_type = SourceType::NODE;
    b.node = std::move(node);
    b.node->add_buffer_reference();
    b.audio_buffer.reset();
    b.network.reset();
    b.callable = {};
}

void PlotProcessor::bind_audio_buffer(uint32_t series_index,
    std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    if (!buffer) {
        MF_ERROR(C, X, "PlotProcessor::bind_audio_buffer: null buffer for series {}", series_index);
        return;
    }
    auto& b = m_bindings[series_index];
    b.source_type = SourceType::AUDIO_BUFFER;
    b.audio_buffer = std::move(buffer);
    b.node.reset();
    b.network.reset();
    b.callable = {};
}

void PlotProcessor::bind_network(uint32_t series_index,
    std::shared_ptr<Nodes::Network::NodeNetwork> network)
{
    if (!network) {
        MF_ERROR(C, X, "PlotProcessor::bind_network: null network for series {}", series_index);
        return;
    }

    const auto mode = network->get_output_mode();
    if (mode != Nodes::Network::OutputMode::AUDIO_SINK
        && mode != Nodes::Network::OutputMode::AUDIO_COMPUTE) {
        MF_ERROR(C, X,
            "PlotProcessor::bind_network: network for series {} has no audio output mode", series_index);
        return;
    }

    auto& b = m_bindings[series_index];
    b.source_type = SourceType::NETWORK;
    b.network = std::move(network);
    b.node.reset();
    b.audio_buffer.reset();
    b.callable = {};
}

void PlotProcessor::bind_callable(uint32_t series_index,
    std::function<void(std::vector<double>&)> fn)
{
    if (!fn) {
        MF_ERROR(C, X, "PlotProcessor::bind_callable: null callable for series {}", series_index);
        return;
    }
    auto& b = m_bindings[series_index];
    b.source_type = SourceType::CALLABLE;
    b.callable = std::move(fn);
    b.node.reset();
    b.audio_buffer.reset();
    b.network.reset();
}

void PlotProcessor::set_raw(uint32_t series_index, std::vector<double> data)
{
    auto& b = m_bindings[series_index];
    b.source_type = SourceType::RAW;
    b.pending_raw = std::move(data);
    b.raw_dirty.test_and_set(std::memory_order_release);
}

void PlotProcessor::unbind(uint32_t series_index)
{
    auto it = m_bindings.find(series_index);
    if (it == m_bindings.end())
        return;
    if (it->second.source_type == SourceType::NODE && it->second.node)
        it->second.node->remove_buffer_reference();
    m_bindings.erase(it);
}

bool PlotProcessor::has_binding(uint32_t series_index) const
{
    return m_bindings.contains(series_index);
}

// =========================================================================
// DataProcessor
// =========================================================================

void PlotProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!std::dynamic_pointer_cast<PlotContainer>(container))
        MF_ERROR(C, X, "PlotProcessor requires a PlotContainer");
}

void PlotProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>&)
{
    for (auto& [idx, b] : m_bindings) {
        if (b.source_type == SourceType::NODE && b.node)
            b.node->remove_buffer_reference();
    }
    m_bindings.clear();
}

void PlotProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto plot = std::dynamic_pointer_cast<PlotContainer>(container);
    if (!plot) {
        MF_ERROR(C, X, "PlotProcessor::process: container is not a PlotContainer");
        return;
    }

    m_processing.store(true, std::memory_order_release);

    for (auto& [idx, b] : m_bindings) {
        if (idx >= plot->series_count())
            continue;

        thread_local std::vector<double> staging;
        staging.resize(plot->series_size(idx));

        {
            auto frame = plot->get_frame(idx);
            if (frame.size() == staging.size()) {
                std::ranges::copy(frame, staging.begin());
            } else {
                std::ranges::fill(staging, 0.0);
            }
        }

        switch (b.source_type) {
        case SourceType::NODE:
            acquire_from_node(b, staging);
            break;
        case SourceType::AUDIO_BUFFER:
            acquire_from_audio_buffer(b, staging);
            break;
        case SourceType::NETWORK:
            acquire_from_network(b, staging);
            break;
        case SourceType::CALLABLE:
            acquire_from_callable(b, staging);
            break;
        case SourceType::RAW:
            acquire_from_raw(b, staging);
            break;
        }

        plot->write_series(idx, staging);
    }

    auto& src = plot->get_data();
    auto& dst = plot->get_processed_data();
    dst.resize(src.size());
    for (size_t i = 0; i < src.size(); ++i)
        dst[i] = src[i];

    m_processing.store(false, std::memory_order_release);
}

// =========================================================================
// Private acquisition
// =========================================================================

void PlotProcessor::acquire_from_node(SeriesBinding& b, std::vector<double>& series)
{
    if (!b.node)
        return;

    auto samples = Buffers::extract_multiple_samples(b.node, series.size());
    const size_t n = std::min(series.size(), samples.size());
    std::copy_n(samples.begin(), n, series.begin());
    if (n < series.size())
        std::fill(series.begin() + static_cast<ptrdiff_t>(n), series.end(), 0.0);
}

void PlotProcessor::acquire_from_audio_buffer(SeriesBinding& b, std::vector<double>& series)
{
    if (!b.audio_buffer)
        return;

    const auto& data = b.audio_buffer->get_data();
    const size_t n = std::min(series.size(), data.size());
    std::copy_n(data.begin(), n, series.begin());
    if (n < series.size())
        std::fill(series.begin() + static_cast<ptrdiff_t>(n), series.end(), 0.0);
}

void PlotProcessor::acquire_from_network(SeriesBinding& b, std::vector<double>& series)
{
    if (!b.network)
        return;

    auto buf = b.network->get_audio_buffer();
    if (!buf)
        return;

    const size_t n = std::min(series.size(), buf->size());
    std::copy_n(buf->begin(), n, series.begin());
    if (n < series.size())
        std::fill(series.begin() + static_cast<ptrdiff_t>(n), series.end(), 0.0);
}

void PlotProcessor::acquire_from_callable(SeriesBinding& b, std::vector<double>& series)
{
    if (b.callable)
        b.callable(series);
}

void PlotProcessor::acquire_from_raw(SeriesBinding& b, std::vector<double>& series)
{
    if (!b.raw_dirty.test(std::memory_order_acquire))
        return;
    b.raw_dirty.clear(std::memory_order_release);
    const size_t n = std::min(series.size(), b.pending_raw.size());
    std::copy_n(b.pending_raw.begin(), n, series.begin());
    if (n < series.size())
        std::fill(series.begin() + static_cast<ptrdiff_t>(n), series.end(), 0.0);
}

} // namespace MayaFlux::Kakshya
