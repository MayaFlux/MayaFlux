#include "SpatialSequence.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {

    void fail(const std::string& message)
    {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "SpatialFileFrames: {}", message);
    }

}

std::shared_ptr<const SpatialFileFrames> SpatialFileFrames::create(const std::string& path, std::string stream)
{
    SpatialReader reader;
    if (!reader.open(path)) {
        return nullptr;
    }

    if (stream.empty()) {
        const auto& names = reader.get_stream_names();
        if (names.size() != 1) {
            fail(names.empty()
                    ? "the file holds no points or curves stream"
                    : "the file holds several streams, so one must be named");
            return nullptr;
        }
        stream = names.front();
    }

    const auto topology = reader.get_topology(stream);
    const size_t count = reader.get_sample_count(stream);
    if (!topology || count == 0) {
        fail("no stream '" + stream + "' with samples");
        return nullptr;
    }

    std::vector<double> times;
    times.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const auto time = reader.sample_time(stream, i);
        if (!time) {
            fail("sample " + std::to_string(i) + " of '" + stream + "' has no time");
            return nullptr;
        }
        times.push_back(*time);
    }

    auto sampling = Kinesis::TimeSampling::acyclic(std::move(times));
    if (!sampling) {
        fail("the sample times of '" + stream + "' do not strictly increase");
        return nullptr;
    }

    return std::shared_ptr<const SpatialFileFrames>(new SpatialFileFrames(
        std::move(reader), std::move(stream), std::move(*sampling), count, *topology));
}

SpatialFileFrames::SpatialFileFrames(SpatialReader reader, std::string stream, Kinesis::TimeSampling times,
    size_t count, Portal::Graphics::PrimitiveTopology topology)
    : m_reader(std::move(reader))
    , m_stream(std::move(stream))
    , m_times(std::move(times))
    , m_count(count)
    , m_topology(topology)
{
}

SpatialFileFrames::~SpatialFileFrames() = default;

std::shared_ptr<const Kakshya::SpatialData> SpatialFileFrames::read(size_t index) const
{
    if (index >= m_count) {
        return nullptr;
    }

    const std::scoped_lock lock(m_mutex);

    if (const auto it = m_live.find(index); it != m_live.end()) {
        if (auto held = it->second.lock()) {
            return held;
        }
    }

    const auto sample = m_reader.read_sample(m_stream, index);
    if (!sample) {
        fail("sample " + std::to_string(index) + " of '" + m_stream + "': " + m_reader.get_last_error());
        return nullptr;
    }

    auto frame = std::make_shared<const Kakshya::SpatialData>(sample->to_data());
    std::erase_if(m_live, [](const auto& entry) { return entry.second.expired(); });
    m_live[index] = frame;
    return frame;
}

std::optional<uint32_t> SpatialFileFrames::max_vertex_count() const
{
    const std::scoped_lock lock(m_mutex);

    if (m_max_vertices) {
        return m_max_vertices;
    }

    size_t most = 0;
    for (size_t i = 0; i < m_count; ++i) {
        const auto vertices = m_reader.get_vertex_count(m_stream, i);
        if (!vertices) {
            fail("the size of sample " + std::to_string(i) + " of '" + m_stream + "' could not be read");
            return std::nullopt;
        }
        most = std::max(most, *vertices);
    }

    m_max_vertices = static_cast<uint32_t>(most);
    return m_max_vertices;
}

std::shared_ptr<Kakshya::SequenceContainer<Kakshya::SpatialData>> open_spatial_sequence(
    const std::string& path, std::string stream)
{
    auto source = SpatialFileFrames::create(path, std::move(stream));
    if (!source) {
        return nullptr;
    }
    return Kakshya::SequenceContainer<Kakshya::SpatialData>::create(
        std::move(source), Kakshya::DataModality::VERTICES_3D);
}

} // namespace MayaFlux::IO
