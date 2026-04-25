#include "StateEncoder.hpp"

#include <cstddef>

#include <cstddef>

#include <cstddef>

#include "MayaFlux/IO/ImageWriter.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    /**
     * EXR layout: width = entity count, height = 3 rows, 4 channels (RGBA)
     * Row 0: position.x, position.y, position.z, intensity
     * Row 1: color.r, color.g, color.b, size
     * Row 2: radius, query_radius, 0, 0
     **/
    constexpr uint32_t k_exr_rows = 3;
    constexpr uint32_t k_channels = 4;

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

    void expand_range(Range& r, float value, bool& initialized)
    {
        if (!initialized) {
            r.min = r.max = value;
            initialized = true;
        } else {
            r.min = std::min(r.min, value);
            r.max = std::max(r.max, value);
        }
    }

    const char* kind_name(Fabric::Kind k)
    {
        switch (k) {
        case Fabric::Kind::Emitter:
            return "emitter";
        case Fabric::Kind::Sensor:
            return "sensor";
        case Fabric::Kind::Agent:
            return "agent";
        }
        return "unknown";
    }

    nlohmann::json encode_wiring(const Fabric& fabric, uint32_t id)
    {
        const Wiring* w = fabric.wiring_for(id);
        if (!w) {
            return { { "kind", "unsupported" } };
        }
        if (!w->move_steps().empty()) {
            nlohmann::json steps = nlohmann::json::array();
            for (const auto& step : w->move_steps()) {
                steps.push_back({ { "x", step.position.x }, { "y", step.position.y },
                    { "z", step.position.z }, { "delay", step.delay_seconds } });
            }
            nlohmann::json wj = { { "kind", "move_to" }, { "steps", std::move(steps) } };
            if (w->times_count() > 1) {
                wj["times"] = w->times_count();
            }
            return wj;
        }
        if (w->interval().has_value()) {
            nlohmann::json wj = { { "kind", "every" }, { "interval", *w->interval() } };
            if (w->duration().has_value()) {
                wj["duration"] = *w->duration();
            }
            if (w->times_count() > 1) {
                wj["times"] = w->times_count();
            }
            return wj;
        }
        return { { "kind", "commit_driven" } };
    }

    struct EntityRecord {
        uint32_t id;
        Fabric::Kind kind;
        glm::vec3 position {};
        float intensity { 0.0F };
        float radius { 0.0F };
        float query_radius { 0.0F };
        std::optional<glm::vec3> color;
        std::optional<float> size;
        std::string influence_fn_name;
        std::string perception_fn_name;
    };

    struct Ranges {
        Range pos_x, pos_y, pos_z;
        Range intensity;
        Range color_r, color_g, color_b;
        Range size;
        Range radius;
        Range query_radius;
    };

} // namespace

bool StateEncoder::encode(const Fabric& fabric, const std::string& base_path)
{
    m_last_error.clear();

    // -------------------------------------------------------------------------
    // Collect all encodable entities (those with a position).
    // -------------------------------------------------------------------------
    std::vector<EntityRecord> records;

    for (uint32_t id : fabric.all_ids()) {
        const auto k = fabric.kind(id);
        switch (k) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(id);
            if (!e || !e->position()) {
                continue;
            }
            if (e->fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Emitter {} has no fn_name", id);
            }
            records.push_back({
                .id = id,
                .kind = k,
                .position = *e->position(),
                .intensity = e->intensity(),
                .radius = e->radius(),
                .color = e->color(),
                .size = e->size(),
                .influence_fn_name = e->fn_name(),
            });
            break;
        }
        case Fabric::Kind::Sensor: {
            auto s = fabric.get_sensor(id);
            if (!s || !s->position()) {
                continue;
            }
            if (s->fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Sensor {} has no fn_name", id);
            }
            records.push_back({
                .id = id,
                .kind = k,
                .position = *s->position(),
                .query_radius = s->query_radius(),
                .perception_fn_name = s->fn_name(),
            });
            break;
        }
        case Fabric::Kind::Agent: {
            auto a = fabric.get_agent(id);
            if (!a || !a->position()) {
                continue;
            }
            if (a->perception_fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Agent {} has no perception_fn_name", id);
            }
            if (a->influence_fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Agent {} has no influence_fn_name", id);
            }
            records.push_back({
                .id = id,
                .kind = k,
                .position = *a->position(),
                .intensity = a->intensity(),
                .radius = a->radius(),
                .query_radius = a->query_radius(),
                .color = a->color(),
                .size = a->size(),
                .influence_fn_name = a->influence_fn_name(),
                .perception_fn_name = a->perception_fn_name(),
            });
            break;
        }
        }
    }

    if (records.empty()) {
        m_last_error = "No entities with positions to encode";
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    // -------------------------------------------------------------------------
    // Compute per-field ranges from the data.
    // -------------------------------------------------------------------------
    Ranges ranges;
    bool init_pos_x = false, init_pos_y = false, init_pos_z = false;
    bool init_intensity = false, init_radius = false, init_query_radius = false;
    bool init_color_r = false, init_color_g = false, init_color_b = false;
    bool init_size = false;

    for (const auto& rec : records) {
        expand_range(ranges.pos_x, rec.position.x, init_pos_x);
        expand_range(ranges.pos_y, rec.position.y, init_pos_y);
        expand_range(ranges.pos_z, rec.position.z, init_pos_z);

        if (rec.kind == Fabric::Kind::Emitter || rec.kind == Fabric::Kind::Agent) {
            expand_range(ranges.intensity, rec.intensity, init_intensity);
            expand_range(ranges.radius, rec.radius, init_radius);
            if (rec.color) {
                expand_range(ranges.color_r, rec.color->r, init_color_r);
                expand_range(ranges.color_g, rec.color->g, init_color_g);
                expand_range(ranges.color_b, rec.color->b, init_color_b);
            }
            if (rec.size) {
                expand_range(ranges.size, *rec.size, init_size);
            }
        }
        if (rec.kind == Fabric::Kind::Sensor || rec.kind == Fabric::Kind::Agent) {
            expand_range(ranges.query_radius, rec.query_radius, init_query_radius);
        }
    }

    // -------------------------------------------------------------------------
    // Build RGBA32F pixel buffer: width=N, height=3.
    // -------------------------------------------------------------------------
    const auto width = static_cast<uint32_t>(records.size());

    IO::ImageData image;
    image.width = width;
    image.height = k_exr_rows;
    image.channels = k_channels;
    image.format = Portal::Graphics::ImageFormat::RGBA32F;

    std::vector<float> pixels(static_cast<size_t>(width) * k_exr_rows * k_channels, 0.0F);

    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];

        const size_t row0 = (static_cast<size_t>(0 * width) + i) * k_channels;
        pixels[row0 + 0] = normalize(rec.position.x, ranges.pos_x);
        pixels[row0 + 1] = normalize(rec.position.y, ranges.pos_y);
        pixels[row0 + 2] = normalize(rec.position.z, ranges.pos_z);
        pixels[row0 + 3] = normalize(rec.intensity, ranges.intensity);

        const size_t row1 = (static_cast<size_t>(1 * width) + i) * k_channels;
        if (rec.color) {
            pixels[row1 + 0] = normalize(rec.color->r, ranges.color_r);
            pixels[row1 + 1] = normalize(rec.color->g, ranges.color_g);
            pixels[row1 + 2] = normalize(rec.color->b, ranges.color_b);
        }
        if (rec.size) {
            pixels[row1 + 3] = normalize(*rec.size, ranges.size);
        }

        const size_t row2 = (static_cast<size_t>(2 * width) + i) * k_channels;
        pixels[row2 + 0] = normalize(rec.radius, ranges.radius);
        pixels[row2 + 1] = normalize(rec.query_radius, ranges.query_radius);
    }

    image.pixels = std::move(pixels);

    // -------------------------------------------------------------------------
    // Write EXR.
    // -------------------------------------------------------------------------
    const std::string exr_path = base_path + ".exr";
    auto writer = IO::ImageWriterRegistry::instance().create_writer(exr_path);
    if (!writer) {
        m_last_error = "No ImageWriter registered for .exr";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    IO::ImageWriteOptions options;
    options.channel_names = { "R", "G", "B", "A" };

    if (!writer->write(exr_path, image, options)) {
        m_last_error = "EXR write failed: " + writer->get_last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    // -------------------------------------------------------------------------
    // Build schema JSON v2.
    // -------------------------------------------------------------------------
    auto make_range_obj = [](const Range& r) {
        return nlohmann::json { { "min", r.min }, { "max", r.max } };
    };

    nlohmann::json entities_arr = nlohmann::json::array();
    for (const auto& rec : records) {
        nlohmann::json ent;
        ent["id"] = rec.id;
        ent["kind"] = kind_name(rec.kind);
        ent["position"] = { rec.position.x, rec.position.y, rec.position.z };

        switch (rec.kind) {
        case Fabric::Kind::Emitter:
            ent["influence_fn_name"] = rec.influence_fn_name;
            ent["intensity"] = rec.intensity;
            ent["radius"] = rec.radius;
            ent["color"] = rec.color
                ? nlohmann::json { rec.color->r, rec.color->g, rec.color->b }
                : nlohmann::json(nullptr);
            ent["size"] = rec.size ? nlohmann::json(*rec.size) : nlohmann::json(nullptr);
            break;
        case Fabric::Kind::Sensor:
            ent["perception_fn_name"] = rec.perception_fn_name;
            ent["query_radius"] = rec.query_radius;
            break;
        case Fabric::Kind::Agent:
            ent["perception_fn_name"] = rec.perception_fn_name;
            ent["influence_fn_name"] = rec.influence_fn_name;
            ent["intensity"] = rec.intensity;
            ent["radius"] = rec.radius;
            ent["query_radius"] = rec.query_radius;
            ent["color"] = rec.color
                ? nlohmann::json { rec.color->r, rec.color->g, rec.color->b }
                : nlohmann::json(nullptr);
            ent["size"] = rec.size ? nlohmann::json(*rec.size) : nlohmann::json(nullptr);
            break;
        }
        ent["wiring"] = encode_wiring(fabric, rec.id);
        entities_arr.push_back(std::move(ent));
    }

    nlohmann::json schema_doc {
        { "version", 3 },
        { "fabric_name", fabric.name() },
        { "entities", std::move(entities_arr) },
        { "ranges", nlohmann::json {
                        { "position.x", make_range_obj(ranges.pos_x) },
                        { "position.y", make_range_obj(ranges.pos_y) },
                        { "position.z", make_range_obj(ranges.pos_z) },
                        { "intensity", make_range_obj(ranges.intensity) },
                        { "color.r", make_range_obj(ranges.color_r) },
                        { "color.g", make_range_obj(ranges.color_g) },
                        { "color.b", make_range_obj(ranges.color_b) },
                        { "size", make_range_obj(ranges.size) },
                        { "radius", make_range_obj(ranges.radius) },
                        { "query_radius", make_range_obj(ranges.query_radius) },
                    } },
    };

    IO::JSONSerializer json_writer;
    const std::string json_path = base_path + ".json";
    if (!json_writer.write(json_path, schema_doc)) {
        m_last_error = "Failed to write schema: " + json_writer.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} entities to {} + {}",
        records.size(), exr_path, json_path);

    return true;
}

} // namespace MayaFlux::Nexus
