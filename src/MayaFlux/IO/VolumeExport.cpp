#include "VolumeExport.hpp"

#include "FileWriter.hpp"

#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::IO {

namespace {

    /**
     * @brief Read one field into the variant its stride implies.
     * @return The populated field, or nullopt if the stride is
     *         unrepresentable or the field is undeclared.
     */
    std::optional<Kakshya::VolumeField> download_field(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        const std::string& name)
    {
        const size_t stride = volume->get_field_stride(name);
        const size_t cells = volume->get_lattice().cell_count();

        Kakshya::VolumeField field;
        field.name = name;
        field.semantics = volume->get_field_semantics(name);

        if (stride == sizeof(float)) {
            std::vector<float> values(cells);
            volume->read_field(name, values.data(), values.size() * sizeof(float));
            field.values = std::move(values);
            return field;
        }

        if (stride == sizeof(glm::vec4)) {
            std::vector<glm::vec4> padded(cells);
            volume->read_field(name, padded.data(), padded.size() * sizeof(glm::vec4));

            std::vector<glm::vec3> values(cells);
            for (size_t i = 0; i < cells; ++i) {
                values[i] = glm::vec3(padded[i]);
            }
            field.values = std::move(values);
            return field;
        }

        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_volume: field '{}' has stride {}, which VolumeData "
            "cannot represent (expected {} or {})",
            name, stride, sizeof(float), sizeof(glm::vec4));
        return std::nullopt;
    }

} // namespace

// ============================================================================
// download_volume
// ============================================================================

std::optional<Kakshya::VolumeData> download_volume(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::vector<std::string>& field_names)
{
    if (!volume) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_volume: null volume");
        return std::nullopt;
    }

    const auto names = field_names.empty() ? volume->get_field_names() : field_names;
    if (names.empty()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_volume: volume declares no fields");
        return std::nullopt;
    }

    Kakshya::VolumeData result;
    result.lattice = volume->get_lattice();
    result.fields.reserve(names.size());

    for (const auto& name : names) {
        if (!volume->has_field(name)) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "download_volume: no field named '{}', skipped", name);
            continue;
        }

        auto field = download_field(volume, name);
        if (field) {
            result.fields.push_back(std::move(*field));
        }
    }

    if (result.fields.empty()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_volume: no field was downloaded");
        return std::nullopt;
    }

    if (!result.is_consistent()) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "download_volume: resulting VolumeData failed is_consistent()");
        return std::nullopt;
    }

    MF_DEBUG(Journal::Component::IO, Journal::Context::FileIO,
        "download_volume: {}x{}x{} lattice, {} of {} fields",
        result.lattice.resolution.x, result.lattice.resolution.y,
        result.lattice.resolution.z, result.fields.size(), names.size());

    return result;
}

// ============================================================================
// save_volume
// ============================================================================

bool save_volume(
    const Kakshya::VolumeData& data,
    const std::string& filepath,
    const VolumeWriteOptions& options)
{
    auto writer = VolumeWriterRegistry::instance().create_writer(filepath);
    if (!writer) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_volume: no writer registered for extension of '{}'", filepath);
        return false;
    }

    const bool ok = writer->write(filepath, data, options);
    if (!ok) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "save_volume: writer failed: {}", writer->get_last_error());
    }
    return ok;
}

bool save_volume(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const std::string& filepath,
    const VolumeWriteOptions& options,
    const std::vector<std::string>& field_names)
{
    auto data = download_volume(volume, field_names);
    if (!data) {
        return false;
    }

    return save_volume(*data, filepath, options);
}

// ============================================================================
// VolumeCapture
// ============================================================================

namespace {
    std::atomic<uint32_t> g_next_capture_id { 1 };
}

VolumeCapture::VolumeCapture(
    Vruta::TaskScheduler& scheduler,
    std::shared_ptr<Buffers::VolumeGridBuffer> volume,
    std::string path_pattern,
    std::vector<std::string> field_names,
    VolumeWriteOptions options,
    VolumeWriteHook write)
    : m_scheduler(scheduler)
    , m_volume(std::move(volume))
    , m_pattern(std::move(path_pattern))
    , m_fields(std::move(field_names))
    , m_options(std::move(options))
    , m_write(write ? std::move(write)
                    : [](Kakshya::VolumeData&& d, const std::string& p,
                          const VolumeWriteOptions& o) {
                          return save_volume(d, p, o);
                      })
{
}

VolumeCapture::~VolumeCapture()
{
    stop();
}

bool VolumeCapture::capture_frame()
{
    if (m_max_frames != 0 && m_frame >= m_max_frames) {
        return false;
    }

    auto data = download_volume(m_volume, m_fields);
    if (!data) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeCapture: readback failed at frame {}", m_frame);
        return false;
    }

    if (!m_write(std::move(*data), resolve_sequence_path(m_pattern, m_frame), m_options)) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeCapture: write failed at frame {}", m_frame);
        return false;
    }

    ++m_frame;
    return true;
}

void VolumeCapture::start(uint32_t max_frames, uint64_t frame_interval)
{
    if (!m_volume) {
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "VolumeCapture: cannot start, null volume");
        return;
    }

    stop();

    m_frame = 0;
    m_max_frames = max_frames;
    m_recording = true;
    m_task_name = "volume_capture_"
        + std::to_string(g_next_capture_id.fetch_add(1, std::memory_order_relaxed));

    auto routine = [](Vruta::TaskScheduler&,
                       VolumeCapture* capture,
                       uint64_t interval) -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate && capture->capture_frame()) {
            co_await Kriya::FrameDelay { .frames_to_wait = interval };
        }
        capture->m_recording = false;
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, this, frame_interval)),
        m_task_name, false);

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VolumeCapture: recording '{}' every {} frame(s), {}",
        m_pattern, frame_interval,
        max_frames == 0 ? std::string("unbounded")
                        : std::format("{} frames", max_frames));
}

void VolumeCapture::stop()
{
    if (!m_recording) {
        return;
    }

    m_scheduler.cancel_task(m_task_name);
    m_task_name.clear();
    m_recording = false;

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "VolumeCapture: stopped after {} frames", m_frame);
}

} // namespace MayaFlux::IO
