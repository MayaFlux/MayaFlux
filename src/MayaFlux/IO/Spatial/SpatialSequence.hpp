#pragma once

#include "SpatialArchive.hpp"

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kakshya/NDData/SpatialData.hpp"
#include "MayaFlux/Kakshya/Semantic/SequenceContainer.hpp"

namespace MayaFlux::IO {

/**
 * @class SpatialFileFrames
 * @brief A FrameSource of Kakshya::SpatialData over one stream of an Alembic archive, read lazily.
 *
 * Frame i is sample i of the stream, copied out of the archive when asked for. The archive
 * is read through a SpatialArchive, whose reader is not thread safe, so reads take turns behind
 * its lock, and any number of threads may call read(). Sources made over the same archive share
 * that one reader. A frame that something still holds is shared by later reads of the same
 * index; a frame nobody holds is read again.
 *
 * Every frame is timed by the archive: the sample times of the stream, which a file that
 * SpatialCache wrote gives as the sample index in seconds. A sample that cannot be read
 * gives nullptr from read() and one logged error.
 */
class MAYAFLUX_API SpatialFileFrames final : public Kakshya::FrameSource<Kakshya::SpatialData> {
public:
    /**
     * @brief Open one stream of an archive.
     * @param path Path to the .abc file.
     * @param stream Name of the points or curves stream. Empty takes the only one a file holds.
     * @return nullptr, with the reason logged, for a file that cannot be read, a stream that
     * is missing or has no samples, a file with several streams when none is named, or sample
     * times that do not strictly increase.
     */
    [[nodiscard]] static std::shared_ptr<const SpatialFileFrames> create(
        const std::string& path, std::string stream = {});

    /**
     * @brief Open one stream of an archive that is already open, sharing its reader.
     * @return nullptr as for a path, and for a null archive.
     */
    [[nodiscard]] static std::shared_ptr<const SpatialFileFrames> create(
        const std::shared_ptr<SpatialArchive>& archive, std::string stream = {});

    ~SpatialFileFrames() override;

    [[nodiscard]] size_t count() const override { return m_count; }

    [[nodiscard]] const Kinesis::TimeSampling& time_sampling() const override { return m_times; }

    [[nodiscard]] std::shared_ptr<const Kakshya::SpatialData> read(size_t index) const override;

    /** @brief The stream's name. */
    [[nodiscard]] const std::string& stream() const noexcept { return m_stream; }

    /** @brief What every frame is: POINT_LIST or LINE_STRIP. */
    [[nodiscard]] Portal::Graphics::PrimitiveTopology topology() const noexcept { return m_topology; }

    /**
     * @brief The most vertices any frame of the stream holds.
     *
     * Read from the size of each sample's positions, so no frame is decoded. The answer is
     * found on the first call and kept.
     *
     * @return The count, or nullopt, with the reason logged, when a sample's size cannot be read.
     */
    [[nodiscard]] std::optional<uint32_t> max_vertex_count() const;

private:
    SpatialFileFrames(std::shared_ptr<SpatialArchive> archive, std::string stream, Kinesis::TimeSampling times,
        size_t count, Portal::Graphics::PrimitiveTopology topology);

    std::shared_ptr<SpatialArchive> m_archive;
    std::string m_stream;
    Kinesis::TimeSampling m_times;
    size_t m_count;
    Portal::Graphics::PrimitiveTopology m_topology;

    mutable std::mutex m_mutex;
    mutable std::optional<uint32_t> m_max_vertices;
    mutable std::unordered_map<size_t, std::weak_ptr<const Kakshya::SpatialData>> m_live;
};

/**
 * @brief Open one stream of an Alembic archive as a container.
 *
 * The container is a SequenceContainer over a SpatialFileFrames, with VERTICES_3D as its
 * modality. A region is a frame range.
 * @return The container, or nullptr as SpatialFileFrames::create.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SequenceContainer<Kakshya::SpatialData>> open_spatial_sequence(
    const std::string& path, std::string stream = {});

} // namespace MayaFlux::IO
