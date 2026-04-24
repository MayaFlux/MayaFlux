#include "StateDecoder.hpp"

#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>
#include <sstream>

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

    // -------------------------------------------------------------------------
    // Hand-rolled JSON parsing for the v0 schema.
    // -------------------------------------------------------------------------
    std::optional<float> parse_float_after(const std::string& line, const std::string& key)
    {
        const auto pos = line.find(key);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        const auto colon = line.find(':', pos + key.size());
        if (colon == std::string::npos) {
            return std::nullopt;
        }
        std::stringstream ss(line.substr(colon + 1));
        float value { 0.0F };
        ss >> value;
        if (!ss) {
            return std::nullopt;
        }
        return value;
    }

    std::optional<std::string> parse_string_after(const std::string& line, const std::string& key)
    {
        const auto pos = line.find(key);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        const auto first_quote = line.find('\"', pos + key.size());
        if (first_quote == std::string::npos) {
            return std::nullopt;
        }
        const auto second_quote = line.find('\"', first_quote + 1);
        if (second_quote == std::string::npos) {
            return std::nullopt;
        }
        return line.substr(first_quote + 1, second_quote - first_quote - 1);
    }

    bool parse_range_line(const std::string& line, Range& out)
    {
        const auto min_val = parse_float_after(line, "\"min\"");
        const auto max_val = parse_float_after(line, "\"max\"");
        if (!min_val || !max_val) {
            return false;
        }
        out.min = *min_val;
        out.max = *max_val;
        return true;
    }

    std::optional<SchemaV1> parse_schema(const std::string& json_path)
    {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        SchemaV1 schema;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("\"version\"") != std::string::npos) {
                if (auto v = parse_float_after(line, "\"version\"")) {
                    schema.version = static_cast<uint32_t>(*v);
                }
            } else if (line.find("\"fabric_name\"") != std::string::npos) {
                if (auto s = parse_string_after(line, "\"fabric_name\"")) {
                    schema.fabric_name = *s;
                }
            } else if (line.find("\"entity_count\"") != std::string::npos) {
                if (auto v = parse_float_after(line, "\"entity_count\"")) {
                    schema.entity_count = static_cast<size_t>(*v);
                }
            } else if (line.find("\"ids\"") != std::string::npos) {
                const auto lbracket = line.find('[');
                const auto rbracket = line.find(']');
                if (lbracket != std::string::npos && rbracket != std::string::npos) {
                    std::string inner = line.substr(lbracket + 1, rbracket - lbracket - 1);
                    std::stringstream ss(inner);
                    std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        try {
                            schema.ids.push_back(static_cast<uint32_t>(std::stoul(tok)));
                        } catch (...) {
                            // Skip malformed tokens.
                        }
                    }
                }
            } else if (line.find("\"position.x\"") != std::string::npos) {
                parse_range_line(line, schema.range_x);
            } else if (line.find("\"position.y\"") != std::string::npos) {
                parse_range_line(line, schema.range_y);
            } else if (line.find("\"position.z\"") != std::string::npos) {
                parse_range_line(line, schema.range_z);
            } else if (line.find("\"intensity\"") != std::string::npos) {
                parse_range_line(line, schema.range_intensity);
            }
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
