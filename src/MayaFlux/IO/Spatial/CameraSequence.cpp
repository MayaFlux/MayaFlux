#include "CameraSequence.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {

    void fail(const std::string& message)
    {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "CameraFileFrames: {}", message);
    }

}

std::shared_ptr<const CameraFileFrames> CameraFileFrames::create(const std::string& path, std::string camera)
{
    return create(SpatialArchive::open(path), std::move(camera));
}

std::shared_ptr<const CameraFileFrames> CameraFileFrames::create(
    const std::shared_ptr<SpatialArchive>& archive, std::string camera)
{
    if (!archive) {
        return nullptr;
    }

    return archive->read([&](SpatialReader& reader) -> std::shared_ptr<const CameraFileFrames> {
        if (camera.empty()) {
            const auto& names = reader.get_camera_names();
            if (names.size() != 1) {
                fail(names.empty()
                        ? "the file holds no camera"
                        : "the file holds several cameras, so one must be named");
                return nullptr;
            }
            camera = names.front();
        }

        const size_t count = reader.get_camera_sample_count(camera);
        if (count == 0) {
            fail("no camera '" + camera + "' with samples");
            return nullptr;
        }

        std::vector<double> times;
        times.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const auto time = reader.camera_time(camera, i);
            if (!time) {
                fail("sample " + std::to_string(i) + " of '" + camera + "' has no time");
                return nullptr;
            }
            times.push_back(*time);
        }

        auto sampling = Kinesis::TimeSampling::acyclic(std::move(times));
        if (!sampling) {
            fail("the sample times of '" + camera + "' do not strictly increase");
            return nullptr;
        }

        return std::shared_ptr<const CameraFileFrames>(
            new CameraFileFrames(archive, std::move(camera), std::move(*sampling), count));
    });
}

CameraFileFrames::CameraFileFrames(std::shared_ptr<SpatialArchive> archive, std::string camera,
    Kinesis::TimeSampling times, size_t count)
    : m_archive(std::move(archive))
    , m_camera(std::move(camera))
    , m_times(std::move(times))
    , m_count(count)
{
}

CameraFileFrames::~CameraFileFrames() = default;

std::shared_ptr<const Kinesis::Lens> CameraFileFrames::read(size_t index) const
{
    if (index >= m_count) {
        return nullptr;
    }

    return m_archive->read([&](SpatialReader& reader) -> std::shared_ptr<const Kinesis::Lens> {
        auto lens = reader.read_lens(m_camera, index);
        if (!lens) {
            fail("sample " + std::to_string(index) + " of '" + m_camera + "': " + reader.get_last_error());
            return nullptr;
        }
        return std::make_shared<const Kinesis::Lens>(*lens);
    });
}

} // namespace MayaFlux::IO
