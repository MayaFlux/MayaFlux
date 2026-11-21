#include "VolumeSequence.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {

    constexpr float k_voxel_tolerance = 1.0E-4F;

    std::optional<std::string> numbered_path(const std::string& pattern, uint64_t number)
    {
        try {
            return std::vformat(pattern, std::make_format_args(number));
        } catch (const std::format_error&) {
            return std::nullopt;
        }
    }

    uint64_t frame_number(const VolumeSequenceOptions& options, size_t index)
    {
        return options.first_number + static_cast<uint64_t>(index) * options.stride;
    }

    bool same_voxel(const glm::vec3& a, const glm::vec3& b)
    {
        for (int axis = 0; axis < 3; ++axis) {
            const float scale = std::max(std::abs(a[axis]), std::abs(b[axis]));
            if (std::abs(a[axis] - b[axis]) > k_voxel_tolerance * scale) {
                return false;
            }
        }
        return true;
    }

    template <typename Signature>
    std::vector<Signature> signature_of(const Kakshya::VolumeData& frame)
    {
        std::vector<Signature> out;
        out.reserve(frame.fields.size());
        for (const auto& field : frame.fields) {
            out.push_back({ field.name, field.is_vector() });
        }
        return out;
    }

    template <typename Signature>
    bool matches(const std::vector<Signature>& signature, const Kakshya::VolumeData& frame)
    {
        if (frame.fields.size() != signature.size()) {
            return false;
        }
        for (size_t i = 0; i < signature.size(); ++i) {
            if (frame.fields[i].name != signature[i].name || frame.fields[i].is_vector() != signature[i].vector) {
                return false;
            }
        }
        return true;
    }

    bool fail(const std::string& message)
    {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO, "VolumeFileSequence: {}", message);
        return false;
    }

}

std::shared_ptr<const VolumeFileSequence> VolumeFileSequence::create(
    VolumeSequenceOptions options, Kinesis::TimeSampling times, VolumeSequenceLattice lattice)
{
    if (options.count == 0 || options.stride == 0) {
        fail("count and stride must be at least 1");
        return nullptr;
    }
    if (!times.time(options.count - 1)) {
        fail("the time sampling cannot time the last frame");
        return nullptr;
    }
    if (const auto stored = times.stored_count(); stored && *stored != options.count) {
        fail("the acyclic time sampling stores a different number of times than there are frames");
        return nullptr;
    }
    const uint64_t last_step = options.count - 1;
    if (last_step > (std::numeric_limits<uint64_t>::max() - options.first_number) / options.stride) {
        fail("the frame numbers overflow");
        return nullptr;
    }
    if (!numbered_path(options.pattern, options.first_number)) {
        fail("the pattern must hold exactly one replacement field a frame number formats into");
        return nullptr;
    }

    const auto load_unpinned = [&options](size_t index) -> std::optional<Kakshya::VolumeData> {
        VolumeReader reader;
        auto frame = reader.load(*numbered_path(options.pattern, frame_number(options, index)), options.read);
        if (!frame) {
            fail("frame " + std::to_string(index) + ": " + reader.get_last_error());
        }
        return frame;
    };

    auto first = load_unpinned(0);
    if (!first) {
        return nullptr;
    }
    auto signature = signature_of<Signature>(*first);

    Kinesis::Lattice3D pinned = first->lattice;

    if (const auto* given = std::get_if<Kinesis::Lattice3D>(&lattice)) {
        if (given->resolution.x == 0 || given->resolution.y == 0 || given->resolution.z == 0) {
            fail("the given lattice has a zero axis");
            return nullptr;
        }
        pinned = *given;
    } else if (std::holds_alternative<UnionLattice>(lattice)) {
        const glm::vec3 voxel = first->lattice.cell_size();
        Kinesis::AABB3D bounds = first->lattice.bounds;
        first.reset();

        for (size_t i = 1; i < options.count; ++i) {
            const auto frame = load_unpinned(i);
            if (!frame) {
                return nullptr;
            }
            if (!matches(signature, *frame)) {
                fail("frame " + std::to_string(i) + " has different fields than frame 0");
                return nullptr;
            }
            if (!same_voxel(frame->lattice.cell_size(), voxel)) {
                fail("frame " + std::to_string(i) + " has a different voxel size than frame 0");
                return nullptr;
            }
            bounds.min = glm::min(bounds.min, frame->lattice.bounds.min);
            bounds.max = glm::max(bounds.max, frame->lattice.bounds.max);
        }

        const glm::vec3 cells = glm::round((bounds.max - bounds.min) / voxel);
        pinned.bounds = bounds;
        pinned.resolution = glm::uvec3(glm::max(cells, glm::vec3(1.0F)));
    }

    return std::shared_ptr<const VolumeFileSequence>(
        new VolumeFileSequence(std::move(options), std::move(times), pinned, std::move(signature)));
}

VolumeFileSequence::VolumeFileSequence(VolumeSequenceOptions options, Kinesis::TimeSampling times,
    Kinesis::Lattice3D lattice, std::vector<Signature> signature)
    : m_options(std::move(options))
    , m_times(std::move(times))
    , m_lattice(lattice)
    , m_signature(std::move(signature))
{
}

VolumeFileSequence::~VolumeFileSequence() = default;

std::string VolumeFileSequence::path(size_t index) const
{
    if (index >= m_options.count) {
        return {};
    }
    return numbered_path(m_options.pattern, frame_number(m_options, index)).value_or(std::string {});
}

std::shared_ptr<const Kakshya::VolumeData> VolumeFileSequence::read(size_t index) const
{
    if (index >= m_options.count) {
        return nullptr;
    }

    {
        const std::scoped_lock lock(m_mutex);
        if (const auto it = m_live.find(index); it != m_live.end()) {
            if (auto held = it->second.lock()) {
                return held;
            }
        }
    }

    VolumeReader reader;
    auto loaded = reader.load(path(index), m_lattice, m_options.read);
    if (!loaded) {
        fail("frame " + std::to_string(index) + ": " + reader.get_last_error());
        return nullptr;
    }
    if (!matches(m_signature, *loaded)) {
        fail("frame " + std::to_string(index) + " has different fields than frame 0");
        return nullptr;
    }
    if (!loaded->is_consistent()) {
        fail("frame " + std::to_string(index) + " is not consistent over the lattice");
        return nullptr;
    }

    auto frame = std::make_shared<const Kakshya::VolumeData>(std::move(*loaded));

    const std::scoped_lock lock(m_mutex);
    if (const auto it = m_live.find(index); it != m_live.end()) {
        if (auto held = it->second.lock()) {
            return held;
        }
    }
    std::erase_if(m_live, [](const auto& entry) { return entry.second.expired(); });
    m_live[index] = frame;
    return frame;
}

std::shared_ptr<Kakshya::SequenceContainer<Kakshya::VolumeData>> open_volume_sequence(
    VolumeSequenceOptions options, Kinesis::TimeSampling times, VolumeSequenceLattice lattice)
{
    auto source = VolumeFileSequence::create(std::move(options), std::move(times), lattice);
    if (!source) {
        return nullptr;
    }
    return Kakshya::SequenceContainer<Kakshya::VolumeData>::create(
        std::move(source), Kakshya::DataModality::VOLUMETRIC_3D);
}

} // namespace MayaFlux::IO
