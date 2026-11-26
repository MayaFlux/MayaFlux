#include "VolumeReflow.hpp"

#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kriya {

namespace {

    constexpr float k_extent_tolerance = 1.0E-4F;

    bool same_lattice(const Kinesis::Lattice3D& a, const Kinesis::Lattice3D& b)
    {
        if (a.resolution != b.resolution) {
            return false;
        }
        const glm::vec3 tolerance = glm::max(glm::abs(a.bounds.extent()), glm::abs(b.bounds.extent())) * k_extent_tolerance;
        return glm::all(glm::lessThanEqual(glm::abs(a.bounds.min - b.bounds.min), tolerance))
            && glm::all(glm::lessThanEqual(glm::abs(a.bounds.max - b.bounds.max), tolerance));
    }

    struct SinkState {
        std::weak_ptr<Buffers::VolumeGridBuffer> volume;
        std::vector<std::vector<float>> scalars;
        std::vector<std::vector<glm::vec4>> padded;
        std::vector<std::string> names;
        std::vector<std::pair<const void*, size_t>> sources;
        bool inconsistent_reported {};
        bool lattice_reported {};
        bool nothing_reported {};
        std::unordered_set<std::string> fields_reported;

        void write(const Kakshya::VolumeData& frame)
        {
            const auto target = volume.lock();
            if (!target) {
                return;
            }

            if (!frame.is_consistent()) {
                if (!inconsistent_reported) {
                    inconsistent_reported = true;
                    MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::BufferProcessing,
                        "volume_sink: the frame is not consistent over its lattice, nothing is written");
                }
                return;
            }
            inconsistent_reported = false;

            if (!same_lattice(frame.lattice, target->get_lattice())) {
                if (!lattice_reported) {
                    lattice_reported = true;
                    MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::BufferProcessing,
                        "volume_sink: the frame's lattice is not the buffer's, nothing is written");
                }
                return;
            }
            lattice_reported = false;

            names.clear();
            sources.clear();
            scalars.resize(frame.fields.size());
            padded.resize(frame.fields.size());

            for (size_t i = 0; i < frame.fields.size(); ++i) {
                const auto& field = frame.fields[i];
                if (!target->has_field(field.name)) {
                    report(field.name, "is not declared on the buffer");
                    continue;
                }

                const size_t stride = target->get_field_stride(field.name);
                if (stride != (field.is_vector() ? sizeof(glm::vec4) : sizeof(float))) {
                    report(field.name, field.is_vector()
                            ? "is a vector but the buffer declares another stride"
                            : "is a scalar but the buffer declares another stride");
                    continue;
                }

                auto narrowed = field.narrowed();
                if (auto* values = std::get_if<std::vector<float>>(&narrowed)) {
                    scalars[i] = std::move(*values);
                    sources.emplace_back(scalars[i].data(), scalars[i].size() * sizeof(float));
                } else {
                    const auto& vectors = std::get<std::vector<glm::vec3>>(narrowed);
                    padded[i].resize(vectors.size());
                    std::ranges::transform(vectors, padded[i].begin(), [](const glm::vec3& v) { return glm::vec4(v, 0.0F); });
                    sources.emplace_back(padded[i].data(), padded[i].size() * sizeof(glm::vec4));
                }
                names.push_back(field.name);
            }

            if (names.empty()) {
                if (!nothing_reported) {
                    nothing_reported = true;
                    MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::BufferProcessing,
                        "volume_sink: no field of the frame could be written");
                }
                return;
            }
            nothing_reported = false;
            target->write_fields(names, sources);
        }

        void report(const std::string& name, const char* reason)
        {
            if (fields_reported.insert(name).second) {
                MF_RT_WARN(Journal::Component::Kriya, Journal::Context::BufferProcessing,
                    "volume_sink: field '{}' {}, left out", name, reason);
            }
        }
    };

}

VolumeReflow::Sink volume_sink(const std::shared_ptr<Buffers::VolumeGridBuffer>& volume)
{
    auto state = std::make_shared<SinkState>();
    state->volume = volume;
    return [state](const std::shared_ptr<const Kakshya::VolumeData>& frame, const FrameContext&) {
        if (frame) {
            state->write(*frame);
        }
    };
}

} // namespace MayaFlux::Kriya
