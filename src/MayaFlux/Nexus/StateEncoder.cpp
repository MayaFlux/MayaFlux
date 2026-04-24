#include "StateEncoder.hpp"

#include "MayaFlux/IO/ImageWriter.hpp"
#include "MayaFlux/IO/TextFileWriter.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

// #include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr size_t k_channels_per_pixel = 4;
    constexpr const char* k_channel_names[k_channels_per_pixel] = {
        "position.x", "position.y", "position.z", "intensity"
    };

    struct Range {
        float min { 0.0F };
        float max { 1.0F };
    };

    float normalize(float value, const Range& r)
    {
        if (r.max <= r.min) {
            return 0.0F;
        }
        return (value - r.min) / (r.max - r.min);
    }

    void update_range(Range& r, float value, bool first)
    {
        if (first) {
            r.min = value;
            r.max = value;
        } else {
            r.min = std::min(r.min, value);
            r.max = std::max(r.max, value);
        }
    }

    std::string format_float(float v)
    {
        std::ostringstream oss;
        oss << std::setprecision(9) << v;
        return oss.str();
    }

    std::string format_range_json(const char* name, const Range& r)
    {
        std::ostringstream oss;
        oss << "    \"" << name << R"(": { "min": )" << format_float(r.min)
            << ", \"max\": " << format_float(r.max) << " }";
        return oss.str();
    }

} // namespace

bool StateEncoder::encode(const Fabric& fabric, const std::string& base_path)
{
    m_last_error.clear();

    // -------------------------------------------------------------------------
    // Collect encodable Emitters (those with a position).
    // -------------------------------------------------------------------------
    struct EmitterRecord {
        uint32_t id;
        glm::vec3 position;
        float intensity;
    };

    std::vector<EmitterRecord> records;
    for (uint32_t id : fabric.all_ids()) {
        if (fabric.kind(id) != Fabric::Kind::Emitter) {
            continue;
        }
        auto emitter = fabric.get_emitter(id);
        if (!emitter || !emitter->position().has_value()) {
            continue;
        }
        records.push_back(EmitterRecord {
            .id = id,
            .position = *emitter->position(),
            .intensity = emitter->intensity(),
        });
    }

    if (records.empty()) {
        m_last_error = "No Emitters with positions to encode";
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    // -------------------------------------------------------------------------
    // Compute per-field ranges from the data.
    // -------------------------------------------------------------------------
    Range ranges[k_channels_per_pixel];
    for (size_t i = 0; i < records.size(); ++i) {
        const bool first = (i == 0);
        update_range(ranges[0], records[i].position.x, first);
        update_range(ranges[1], records[i].position.y, first);
        update_range(ranges[2], records[i].position.z, first);
        update_range(ranges[3], records[i].intensity, first);
    }

    // -------------------------------------------------------------------------
    // Build RGBA32F ImageData: 1 row, N columns, one pixel per Emitter.
    // -------------------------------------------------------------------------
    const auto width = static_cast<uint32_t>(records.size());
    const uint32_t height = 1;

    IO::ImageData image;
    image.width = width;
    image.height = height;
    image.channels = k_channels_per_pixel;
    image.format = Portal::Graphics::ImageFormat::RGBA32F;

    std::vector<float> pixels(static_cast<size_t>(width) * height * k_channels_per_pixel);
    for (size_t i = 0; i < records.size(); ++i) {
        pixels[i * k_channels_per_pixel + 0] = normalize(records[i].position.x, ranges[0]);
        pixels[i * k_channels_per_pixel + 1] = normalize(records[i].position.y, ranges[1]);
        pixels[i * k_channels_per_pixel + 2] = normalize(records[i].position.z, ranges[2]);
        pixels[i * k_channels_per_pixel + 3] = normalize(records[i].intensity, ranges[3]);
    }
    image.pixels = std::move(pixels);

    // -------------------------------------------------------------------------
    // Write EXR via ImageWriterRegistry.
    // -------------------------------------------------------------------------
    const std::string exr_path = base_path + ".exr";
    auto writer = IO::ImageWriterRegistry::instance().create_writer(exr_path);
    if (!writer) {
        m_last_error = "No ImageWriter registered for .exr";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    IO::ImageWriteOptions options;
    options.channel_names = {
        k_channel_names[0], k_channel_names[1],
        k_channel_names[2], k_channel_names[3]
    };

    if (!writer->write(exr_path, image, options)) {
        m_last_error = "EXR write failed: " + writer->get_last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    // -------------------------------------------------------------------------
    // Write schema JSON by hand.
    // -------------------------------------------------------------------------
    const std::string json_path = base_path + ".json";
    IO::TextFileWriter schema_writer;
    if (!schema_writer.open(json_path,
            IO::FileWriteOptions::CREATE | IO::FileWriteOptions::TRUNCATE)) {
        m_last_error = "Failed to open schema file: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": 1,\n";
    out << R"(  "fabric_name": ")" << fabric.name() << "\",\n";
    out << "  \"entity_count\": " << records.size() << ",\n";

    out << "  \"ids\": [";
    for (size_t i = 0; i < records.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << records[i].id;
    }
    out << "],\n";

    out << "  \"ranges\": {\n";
    for (size_t i = 0; i < k_channels_per_pixel; ++i) {
        out << format_range_json(k_channel_names[i], ranges[i]);
        if (i + 1 < k_channels_per_pixel) {
            out << ",";
        }
        out << "\n";
    }
    out << "  }\n";
    out << "}\n";

    if (!schema_writer.write_string(out.str())) {
        m_last_error = "Failed to write schema file: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    schema_writer.close();

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} emitters to {} + {}",
        records.size(), exr_path, json_path);

    return true;
}

} // namespace MayaFlux::Nexus
