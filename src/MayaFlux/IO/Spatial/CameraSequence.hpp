#pragma once

#include "SpatialArchive.hpp"

#include "MayaFlux/Kakshya/FrameSource.hpp"

namespace MayaFlux::IO {

/**
 * @class CameraFileFrames
 * @brief A FrameSource of Kinesis::Lens over one camera object of an Alembic archive, read lazily.
 *
 * Frame i is sample i of the camera: its focal length, film back, clip planes and screen window,
 * as SpatialReader::read_lens gives them. The camera's placement is not a frame of this source:
 * the camera sits under transform objects, so its world placement belongs to the row that shows it.
 * The archive is read through a SpatialArchive, so sources over one archive share its reader and
 * any number of threads may call read(). Every frame is timed by the archive, and a sample that
 * cannot be read gives nullptr and one logged error.
 */
class MAYAFLUX_API CameraFileFrames final : public Kakshya::FrameSource<Kinesis::Lens> {
public:
    /**
     * @brief Open one camera object of an archive.
     * @param path Path to the .abc file.
     * @param camera Path of the camera object. Empty takes the only one a file holds.
     * @return nullptr, with the reason logged, for a file that cannot be read, a camera that is
     * missing or has no samples, a file with several cameras when none is named, or sample times
     * that do not strictly increase.
     */
    [[nodiscard]] static std::shared_ptr<const CameraFileFrames> create(
        const std::string& path, std::string camera = {});

    /** @brief Open one camera object of an archive that is already open, sharing its reader. */
    [[nodiscard]] static std::shared_ptr<const CameraFileFrames> create(
        const std::shared_ptr<SpatialArchive>& archive, std::string camera = {});

    ~CameraFileFrames() override;

    [[nodiscard]] size_t count() const override { return m_count; }

    [[nodiscard]] const Kinesis::TimeSampling& time_sampling() const override { return m_times; }

    [[nodiscard]] std::shared_ptr<const Kinesis::Lens> read(size_t index) const override;

    /** @brief The camera object's path. */
    [[nodiscard]] const std::string& camera() const noexcept { return m_camera; }

private:
    CameraFileFrames(std::shared_ptr<SpatialArchive> archive, std::string camera,
        Kinesis::TimeSampling times, size_t count);

    std::shared_ptr<SpatialArchive> m_archive;
    std::string m_camera;
    Kinesis::TimeSampling m_times;
    size_t m_count;
};

} // namespace MayaFlux::IO
