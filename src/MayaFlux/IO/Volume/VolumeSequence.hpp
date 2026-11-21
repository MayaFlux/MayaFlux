#pragma once

#include "MayaFlux/IO/Volume/VolumeReader.hpp"
#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kakshya/Semantic/SequenceContainer.hpp"

namespace MayaFlux::IO {

/**
 * @enum LatticeSource
 * @brief Where the one lattice of a volume sequence comes from, when it is not given.
 *
 * A VolumeGridBuffer fixes its lattice at construction, so a sequence has exactly one.
 * FirstFrame takes the lattice the first file's content fills, and content of later frames
 * outside that extent is not read: choose it when the first frame bounds the whole
 * sequence. Union reads every file once when the sequence is made, one at a time, to find
 * the extent, and needs one voxel size across the files: choose it when the content grows
 * or moves. There is no default: the caller says which, or passes a Lattice3D.
 */
enum class LatticeSource : uint8_t {
    FirstFrame,
    Union
};

/** @brief A lattice source or a lattice of the caller's own. */
using VolumeSequenceLattice = std::variant<LatticeSource, Kinesis::Lattice3D>;

/**
 * @struct VolumeSequenceOptions
 * @brief A numbered sequence of volume files and how to read it.
 */
struct VolumeSequenceOptions {
    /**
     * @brief File name with one std::format replacement field for the frame number,
     *        such as "smoke.{:04}.vdb".
     */
    std::string pattern;

    uint64_t first_number {}; ///< Number substituted for frame 0.
    uint64_t stride { 1 }; ///< Difference between the numbers of consecutive frames. At least 1.
    size_t count {}; ///< Number of frames. At least 1.

    VolumeReadOptions read; ///< Grids to read from every file.
};

/**
 * @class VolumeFileSequence
 * @brief A FrameSource of VolumeData over numbered .vdb files, read lazily.
 *
 * Frame i is the file the pattern names at first_number + i * stride, read onto the
 * sequence's one lattice. Each file is read when asked for, by a reader of its own,
 * so any number of threads may read. A frame that something still holds is shared by
 * later reads of the same index; a frame nobody holds is read again.
 *
 * Every frame has the field names and kinds (scalar or vector) the first file had,
 * in the same order, because the buffer a frame is written into declares them once.
 * A file that differs, or is missing or unreadable, gives nullptr from read() and one
 * logged error.
 */
class MAYAFLUX_API VolumeFileSequence final : public Kakshya::FrameSource<Kakshya::VolumeData> {
public:
    /**
     * @brief Open a sequence.
     * @param options The files and the grids to read.
     * @param times The time of each frame. A .vdb carries none, so the caller sets it. An
     * acyclic sampling must store exactly options.count times.
     * @param lattice Where the lattice every frame is read onto comes from. Required.
     * @return nullptr, with the reason logged, when the options are unusable, the first
     * file cannot be read, a LatticeSource::Union pre-pass meets an unreadable file or files
     * that disagree on voxel size or fields, or a given lattice has a zero axis.
     */
    [[nodiscard]] static std::shared_ptr<const VolumeFileSequence> create(
        VolumeSequenceOptions options, Kinesis::TimeSampling times, VolumeSequenceLattice lattice);

    ~VolumeFileSequence() override;

    [[nodiscard]] size_t count() const override { return m_options.count; }

    [[nodiscard]] const Kinesis::TimeSampling& time_sampling() const override { return m_times; }

    [[nodiscard]] std::shared_ptr<const Kakshya::VolumeData> read(size_t index) const override;

    /** @brief The lattice every frame is read onto. */
    [[nodiscard]] const Kinesis::Lattice3D& lattice() const noexcept { return m_lattice; }

    /** @brief The path of a frame, as the pattern names it, or empty past the end. */
    [[nodiscard]] std::string path(size_t index) const;

private:
    struct Signature {
        std::string name;
        bool vector {};
    };

    VolumeFileSequence(VolumeSequenceOptions options, Kinesis::TimeSampling times,
        Kinesis::Lattice3D lattice, std::vector<Signature> signature);

    VolumeSequenceOptions m_options;
    Kinesis::TimeSampling m_times;
    Kinesis::Lattice3D m_lattice;
    std::vector<Signature> m_signature;

    mutable std::mutex m_mutex;
    mutable std::unordered_map<size_t, std::weak_ptr<const Kakshya::VolumeData>> m_live;
};

/**
 * @brief Open a numbered volume sequence as a container.
 *
 * The container is a SequenceContainer over a VolumeFileSequence, with VOLUMETRIC_3D
 * as its modality. A region is a frame range.
 * @return The container, or nullptr as VolumeFileSequence::create.
 */
[[nodiscard]] MAYAFLUX_API std::shared_ptr<Kakshya::SequenceContainer<Kakshya::VolumeData>> open_volume_sequence(
    VolumeSequenceOptions options, Kinesis::TimeSampling times, VolumeSequenceLattice lattice);

} // namespace MayaFlux::IO
