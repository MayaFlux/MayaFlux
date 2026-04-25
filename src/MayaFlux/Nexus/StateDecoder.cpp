#include "StateDecoder.hpp"

#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr uint32_t k_exr_rows = 3;
    constexpr uint32_t k_channels = 4;

    struct Range {
        float min { 0.0F };
        float max { 1.0F };
    };

    float denormalize(float norm, const Range& r)
    {
        return r.min + norm * (r.max - r.min);
    }

    struct Ranges {
        Range pos_x, pos_y, pos_z;
        Range intensity;
        Range color_r, color_g, color_b;
        Range size;
        Range radius;
        Range query_radius;
    };

    struct EntityEntry {
        uint32_t id;
        Fabric::Kind kind;
        std::string influence_fn_name;
        std::string perception_fn_name;
        bool has_color { false };
        bool has_size { false };
    };

    struct SchemaV2 {
        std::string fabric_name;
        std::vector<EntityEntry> entities;
        Ranges ranges;
    };

    Range parse_range(const nlohmann::json& ranges, const char* name)
    {
        if (!ranges.contains(name)) {
            return {};
        }
        const auto& r = ranges.at(name);
        return { r.at("min").get<float>(), r.at("max").get<float>() };
    }

    std::optional<SchemaV2> load_schema(const std::string& json_path)
    {
        IO::JSONSerializer serializer;
        auto doc_opt = serializer.read(json_path);
        if (!doc_opt) {
            return std::nullopt;
        }
        const auto& doc = *doc_opt;

        SchemaV2 schema;
        try {
            if (doc.at("version").get<uint32_t>() != 2) {
                return std::nullopt;
            }
            schema.fabric_name = doc.at("fabric_name").get<std::string>();

            const auto& ranges = doc.at("ranges");
            schema.ranges.pos_x = parse_range(ranges, "position.x");
            schema.ranges.pos_y = parse_range(ranges, "position.y");
            schema.ranges.pos_z = parse_range(ranges, "position.z");
            schema.ranges.intensity = parse_range(ranges, "intensity");
            schema.ranges.color_r = parse_range(ranges, "color.r");
            schema.ranges.color_g = parse_range(ranges, "color.g");
            schema.ranges.color_b = parse_range(ranges, "color.b");
            schema.ranges.size = parse_range(ranges, "size");
            schema.ranges.radius = parse_range(ranges, "radius");
            schema.ranges.query_radius = parse_range(ranges, "query_radius");

            for (const auto& ent : doc.at("entities")) {
                const std::string kind_str = ent.at("kind").get<std::string>();
                Fabric::Kind kind;
                if (kind_str == "emitter") {
                    kind = Fabric::Kind::Emitter;
                } else if (kind_str == "sensor") {
                    kind = Fabric::Kind::Sensor;
                } else if (kind_str == "agent") {
                    kind = Fabric::Kind::Agent;
                } else {
                    continue;
                }

                EntityEntry entry;
                entry.id = ent.at("id").get<uint32_t>();
                entry.kind = kind;

                if (ent.contains("influence_fn_name")) {
                    entry.influence_fn_name = ent.at("influence_fn_name").get<std::string>();
                }
                if (ent.contains("perception_fn_name")) {
                    entry.perception_fn_name = ent.at("perception_fn_name").get<std::string>();
                }
                entry.has_color = ent.contains("color") && !ent.at("color").is_null();
                entry.has_size = ent.contains("size") && !ent.at("size").is_null();

                schema.entities.push_back(std::move(entry));
            }
        } catch (const nlohmann::json::exception&) {
            return std::nullopt;
        }

        if (schema.entities.empty()) {
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
    // Load and validate schema.
    // -------------------------------------------------------------------------
    auto schema_opt = load_schema(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema v2: " + json_path;
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

    if (image.channels != k_channels) {
        m_last_error = "EXR has " + std::to_string(image.channels)
            + " channels, expected " + std::to_string(k_channels);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (image.height != k_exr_rows) {
        m_last_error = "EXR has " + std::to_string(image.height)
            + " rows, expected " + std::to_string(k_exr_rows);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (image.width != static_cast<uint32_t>(schema.entities.size())) {
        m_last_error = "EXR width (" + std::to_string(image.width)
            + ") does not match entity count ("
            + std::to_string(schema.entities.size()) + ")";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const uint32_t width = image.width;
    const auto& r = schema.ranges;

    // -------------------------------------------------------------------------
    // Patch each entity by id and kind.
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < schema.entities.size(); ++i) {
        const auto& entry = schema.entities[i];

        const size_t row0 = (0 * width + i) * k_channels;
        const size_t row1 = (1 * width + i) * k_channels;
        const size_t row2 = (2 * width + i) * k_channels;

        const glm::vec3 position {
            denormalize((*pixels)[row0 + 0], r.pos_x),
            denormalize((*pixels)[row0 + 1], r.pos_y),
            denormalize((*pixels)[row0 + 2], r.pos_z),
        };

        switch (entry.kind) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(entry.id);
            if (!e) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: id {} not found as Emitter, skipping", entry.id);
                ++m_missing_count;
                continue;
            }
            if (!entry.influence_fn_name.empty()
                && e->fn_name() != entry.influence_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Emitter {} fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.influence_fn_name, e->fn_name());
            }
            e->set_position(position);
            e->set_intensity(denormalize((*pixels)[row0 + 3], r.intensity));
            if (entry.has_color) {
                e->set_color(glm::vec3 {
                    denormalize((*pixels)[row1 + 0], r.color_r),
                    denormalize((*pixels)[row1 + 1], r.color_g),
                    denormalize((*pixels)[row1 + 2], r.color_b),
                });
            }
            if (entry.has_size) {
                e->set_size(denormalize((*pixels)[row1 + 3], r.size));
            }
            e->set_radius(denormalize((*pixels)[row2 + 0], r.radius));
            break;
        }
        case Fabric::Kind::Sensor: {
            auto s = fabric.get_sensor(entry.id);
            if (!s) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: id {} not found as Sensor, skipping", entry.id);
                ++m_missing_count;
                continue;
            }
            if (!entry.perception_fn_name.empty()
                && s->fn_name() != entry.perception_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Sensor {} fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.perception_fn_name, s->fn_name());
            }
            s->set_position(position);
            s->set_query_radius(denormalize((*pixels)[row2 + 1], r.query_radius));
            break;
        }
        case Fabric::Kind::Agent: {
            auto a = fabric.get_agent(entry.id);
            if (!a) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: id {} not found as Agent, skipping", entry.id);
                ++m_missing_count;
                continue;
            }
            if (!entry.perception_fn_name.empty()
                && a->perception_fn_name() != entry.perception_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Agent {} perception_fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.perception_fn_name, a->perception_fn_name());
            }
            if (!entry.influence_fn_name.empty()
                && a->influence_fn_name() != entry.influence_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Agent {} influence_fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.influence_fn_name, a->influence_fn_name());
            }
            a->set_position(position);
            a->set_intensity(denormalize((*pixels)[row0 + 3], r.intensity));
            if (entry.has_color) {
                a->set_color(glm::vec3 {
                    denormalize((*pixels)[row1 + 0], r.color_r),
                    denormalize((*pixels)[row1 + 1], r.color_g),
                    denormalize((*pixels)[row1 + 2], r.color_b),
                });
            }
            if (entry.has_size) {
                a->set_size(denormalize((*pixels)[row1 + 3], r.size));
            }
            a->set_radius(denormalize((*pixels)[row2 + 0], r.radius));
            a->set_query_radius(denormalize((*pixels)[row2 + 1], r.query_radius));
            break;
        }
        }

        ++m_patched_count;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder: patched {} entities ({} missing) from {} + {}",
        m_patched_count, m_missing_count, exr_path, json_path);

    return true;
}

} // namespace MayaFlux::Nexus
