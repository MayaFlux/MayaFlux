#pragma once

#include "SpatialArchive.hpp"

#include "MayaFlux/Kakshya/FrameSource.hpp"

namespace MayaFlux::IO {

/**
 * @class MeshFileFrames
 * @brief A FrameSource of Kakshya::MeshData over one polygon or subdivision mesh object of an
 *        Alembic archive, read lazily.
 *
 * Frame i is sample i of the object, read as SpatialReader::read_mesh reads it: triangles,
 * wound counter clockwise, normals and texture coordinates as the file has them or derived. The
 * archive is read through a SpatialArchive, whose reader is not thread safe, so reads take turns
 * behind its lock, and any number of threads may call read(). Sources made over the same archive
 * share that one reader. A frame that something still holds is shared by later reads of the same
 * index; a frame nobody holds is read again.
 *
 * Every frame is timed by the archive. A sample that cannot be read gives nullptr from read()
 * and one logged error.
 *
 * variance() says what the file reports about how the connectivity changes, so a consumer
 * knows whether the indices of every frame are the same.
 */
class MAYAFLUX_API MeshFileFrames final : public Kakshya::FrameSource<Kakshya::MeshData> {
public:
    /**
     * @brief Open one mesh object of an archive.
     * @param path Path to the .abc file.
     * @param mesh Path of the polygon or subdivision mesh object. Empty takes the only one a
     *        file holds.
     * @return nullptr, with the reason logged, for a file that cannot be read, a mesh that is
     * missing or has no samples, a file with several meshes when none is named, or sample times
     * that do not strictly increase.
     */
    [[nodiscard]] static std::shared_ptr<const MeshFileFrames> create(
        const std::string& path, std::string mesh = {});

    /**
     * @brief Open one mesh object of an archive that is already open, sharing its reader.
     * @return nullptr as for a path, and for a null archive.
     */
    [[nodiscard]] static std::shared_ptr<const MeshFileFrames> create(
        const std::shared_ptr<SpatialArchive>& archive, std::string mesh = {});

    ~MeshFileFrames() override;

    [[nodiscard]] size_t count() const override { return m_count; }

    [[nodiscard]] const Kinesis::TimeSampling& time_sampling() const override { return m_times; }

    [[nodiscard]] std::shared_ptr<const Kakshya::MeshData> read(size_t index) const override;

    /** @brief The mesh object's path. */
    [[nodiscard]] const std::string& mesh() const noexcept { return m_mesh; }

    /** @brief How the connectivity changes from frame to frame, as the file reports it. */
    [[nodiscard]] Kakshya::TopologyVariance variance() const noexcept { return m_variance; }

private:
    MeshFileFrames(std::shared_ptr<SpatialArchive> archive, std::string mesh, Kinesis::TimeSampling times,
        size_t count, Kakshya::TopologyVariance variance);

    std::shared_ptr<SpatialArchive> m_archive;
    std::string m_mesh;
    Kinesis::TimeSampling m_times;
    size_t m_count;
    Kakshya::TopologyVariance m_variance;

    mutable std::mutex m_mutex;
    mutable std::unordered_map<size_t, std::weak_ptr<const Kakshya::MeshData>> m_live;
};

} // namespace MayaFlux::IO
