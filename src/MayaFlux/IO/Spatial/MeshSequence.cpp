#include "MeshSequence.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {

    void fail(const std::string& message)
    {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "MeshFileFrames: {}", message);
    }

}

std::shared_ptr<const MeshFileFrames> MeshFileFrames::create(const std::string& path, std::string mesh)
{
    return create(SpatialArchive::open(path), std::move(mesh));
}

std::shared_ptr<const MeshFileFrames> MeshFileFrames::create(
    const std::shared_ptr<SpatialArchive>& archive, std::string mesh)
{
    if (!archive) {
        return nullptr;
    }

    return archive->read([&](SpatialReader& reader) -> std::shared_ptr<const MeshFileFrames> {
    if (mesh.empty()) {
        const auto& names = reader.get_mesh_names();
        if (names.size() != 1) {
            fail(names.empty()
                    ? "the file holds no polygon or subdivision mesh"
                    : "the file holds several meshes, so one must be named");
            return nullptr;
        }
        mesh = names.front();
    }

    const auto variance = reader.get_mesh_variance(mesh);
    const size_t count = reader.get_mesh_sample_count(mesh);
    if (!variance || count == 0) {
        fail("no mesh '" + mesh + "' with samples");
        return nullptr;
    }

    std::vector<double> times;
    times.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const auto time = reader.mesh_time(mesh, i);
        if (!time) {
            fail("sample " + std::to_string(i) + " of '" + mesh + "' has no time");
            return nullptr;
        }
        times.push_back(*time);
    }

    auto sampling = Kinesis::TimeSampling::acyclic(std::move(times));
    if (!sampling) {
        fail("the sample times of '" + mesh + "' do not strictly increase");
        return nullptr;
    }

    return std::shared_ptr<const MeshFileFrames>(new MeshFileFrames(
        archive, std::move(mesh), std::move(*sampling), count, *variance));
    });
}

MeshFileFrames::MeshFileFrames(std::shared_ptr<SpatialArchive> archive, std::string mesh, Kinesis::TimeSampling times,
    size_t count, Kakshya::TopologyVariance variance)
    : m_archive(std::move(archive))
    , m_mesh(std::move(mesh))
    , m_times(std::move(times))
    , m_count(count)
    , m_variance(variance)
{
}

MeshFileFrames::~MeshFileFrames() = default;

std::shared_ptr<const Kakshya::MeshData> MeshFileFrames::read(size_t index) const
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

    auto frame = m_archive->read([&](SpatialReader& reader) -> std::shared_ptr<const Kakshya::MeshData> {
        auto mesh = reader.read_mesh(m_mesh, index);
        if (!mesh) {
            fail("sample " + std::to_string(index) + " of '" + m_mesh + "': " + reader.get_last_error());
            return nullptr;
        }
        return std::make_shared<const Kakshya::MeshData>(std::move(*mesh));
    });
    if (!frame) {
        return nullptr;
    }
    std::erase_if(m_live, [](const auto& entry) { return entry.second.expired(); });
    m_live[index] = frame;
    return frame;
}

} // namespace MayaFlux::IO
