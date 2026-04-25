#include "StateDecoder.hpp"

#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr size_t k_channels_per_pixel = 4;

    struct Range {
        float min { 0.0F };
        float max { 1.0F };
    };

    float denormalize(float norm, const Range& r)
    {
        return r.min + norm * (r.max - r.min);
    }

    struct SchemaV1 {
        uint32_t version { 0 };
        std::string fabric_name;
        size_t entity_count { 0 };
        std::vector<uint32_t> ids;
        Range range_x;
        Range range_y;
        Range range_z;
        Range range_intensity;
    };

    std::optional<SchemaV1> parse_schema(const std::string& json_path)
    {
        IO::JSONSerializer serializer;
        auto doc_opt = serializer.read(json_path);
        if (!doc_opt) {
            return std::nullopt;
        }
        const auto& doc = *doc_opt;

        SchemaV1 schema;
        try {
            schema.version = doc.at("version").get<uint32_t>();
            schema.fabric_name = doc.at("fabric_name").get<std::string>();
            schema.entity_count = doc.at("entity_count").get<size_t>();

            for (const auto& id : doc.at("ids")) {
                schema.ids.push_back(id.get<uint32_t>());
            }

            auto parse_range = [&](const char* name, Range& r) {
                const auto& ranges = doc.at("ranges");
                if (!ranges.contains(name)) {
                    return;
                }
                r.min = ranges.at(name).at("min").get<float>();
                r.max = ranges.at(name).at("max").get<float>();
            };
            parse_range("position.x", schema.range_x);
            parse_range("position.y", schema.range_y);
            parse_range("position.z", schema.range_z);
            parse_range("intensity", schema.range_intensity);
        } catch (const nlohmann::json::exception&) {
            return std::nullopt;
        }

        if (schema.version != 1 || schema.ids.empty()) {
            return std::nullopt;
        }
        return schema;
    }

} // namespace

bool StateDecoder::decode(Fabric& fabric, const std::string& base_path)
{
    m_last_error.clear();
    m_patched_count = 0;
    m_missing_count = 0;

    const std::string json_path = base_path + ".json";
    const std::string exr_path = base_path + ".exr";

    // -------------------------------------------------------------------------
    // Parse schema.
    // -------------------------------------------------------------------------
    auto schema_opt = parse_schema(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to parse schema: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    const auto& schema = *schema_opt;

    // -------------------------------------------------------------------------
    // Load EXR pixels.
    // -------------------------------------------------------------------------
    auto image_opt = IO::ImageReader::load(exr_path, 0);
    if (!image_opt) {
        m_last_error = "Failed to load EXR: " + exr_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    const auto& image = *image_opt;

    const auto* pixels = image.as_float();
    if (!pixels || pixels->empty()) {
        m_last_error = "EXR has no float pixel data: " + exr_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (image.channels != k_channels_per_pixel) {
        m_last_error = "EXR has " + std::to_string(image.channels)
            + " channels, expected " + std::to_string(k_channels_per_pixel);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const size_t pixel_count = static_cast<size_t>(image.width) * image.height;
    if (pixel_count != schema.ids.size()) {
        m_last_error = "Pixel count (" + std::to_string(pixel_count)
            + ") does not match schema ids count ("
            + std::to_string(schema.ids.size()) + ")";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    // -------------------------------------------------------------------------
    // Patch each Emitter by id.
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < schema.ids.size(); ++i) {
        const uint32_t id = schema.ids[i];

        auto emitter = fabric.get_emitter(id);
        if (!emitter) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                "StateDecoder: id {} not found in fabric as Emitter, skipping", id);
            ++m_missing_count;
            continue;
        }

        const size_t base = i * k_channels_per_pixel;
        const float nx = (*pixels)[base + 0];
        const float ny = (*pixels)[base + 1];
        const float nz = (*pixels)[base + 2];
        const float ni = (*pixels)[base + 3];

        emitter->set_position(glm::vec3 {
            denormalize(nx, schema.range_x),
            denormalize(ny, schema.range_y),
            denormalize(nz, schema.range_z),
        });
        emitter->set_intensity(denormalize(ni, schema.range_intensity));

        ++m_patched_count;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder: patched {} emitters ({} missing) from {} + {}",
        m_patched_count, m_missing_count, exr_path, json_path);

    return true;
}

} // namespace MayaFlux::Nexus
