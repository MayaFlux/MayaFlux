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
        std::vector<glm::vec4> padded;
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

            size_t written = 0;
            for (const auto& field : frame.fields) {
                if (!target->has_field(field.name)) {
                    report(field.name, "is not declared on the buffer");
                    continue;
                }

                const size_t stride = target->get_field_stride(field.name);
                if (const auto* scalars = field.as_scalar()) {
                    if (stride != sizeof(float)) {
                        report(field.name, "is a scalar but the buffer declares another stride");
                        continue;
                    }
                    target->seed_raw(field.name, scalars->data(), scalars->size() * sizeof(float));
                    ++written;
                    continue;
                }

                const auto* vectors = field.as_vector();
                if (stride != sizeof(glm::vec4)) {
                    report(field.name, "is a vector but the buffer declares another stride");
                    continue;
                }
                padded.resize(vectors->size());
                std::ranges::transform(*vectors, padded.begin(), [](const glm::vec3& v) { return glm::vec4(v, 0.0F); });
                target->seed_raw(field.name, padded.data(), padded.size() * sizeof(glm::vec4));
                ++written;
            }

            if (written == 0 && !nothing_reported) {
                nothing_reported = true;
                MF_RT_ERROR(Journal::Component::Kriya, Journal::Context::BufferProcessing,
                    "volume_sink: no field of the frame could be written");
            } else if (written > 0) {
                nothing_reported = false;
            }
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
