#include "StateEncoder.hpp"

#include "Sinks.hpp"

#include "MayaFlux/IO/ImageWriter.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr uint32_t k_exr_rows = 5;
    constexpr uint32_t k_channels = 4;

    // -------------------------------------------------------------------------
    // Schema structs
    // -------------------------------------------------------------------------

    struct Range {
        float min { 0.0F };
        float max { 1.0F };

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("min", &Range::min),
                IO::member("max", &Range::max));
        }
    };

    struct RangeSet {
        Range pos_x, pos_y, pos_z;
        Range intensity;
        Range color_r, color_g, color_b;
        Range size;
        Range radius;
        Range query_radius;

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("position.x", &RangeSet::pos_x),
                IO::member("position.y", &RangeSet::pos_y),
                IO::member("position.z", &RangeSet::pos_z),
                IO::member("intensity", &RangeSet::intensity),
                IO::member("color.r", &RangeSet::color_r),
                IO::member("color.g", &RangeSet::color_g),
                IO::member("color.b", &RangeSet::color_b),
                IO::member("size", &RangeSet::size),
                IO::member("radius", &RangeSet::radius),
                IO::member("query_radius", &RangeSet::query_radius));
        }
    };

    struct WiringStep {
        glm::vec3 position {};
        double delay_seconds { 0.0 };

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("position", &WiringStep::position),
                IO::member("delay", &WiringStep::delay_seconds));
        }
    };

    struct WiringRecord {
        std::string kind { "commit_driven" };
        std::optional<double> interval;
        std::optional<double> duration;
        std::optional<size_t> times;
        std::optional<std::vector<WiringStep>> steps;

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("kind", &WiringRecord::kind),
                IO::opt_member("interval", &WiringRecord::interval),
                IO::opt_member("duration", &WiringRecord::duration),
                IO::opt_member("times", &WiringRecord::times),
                IO::opt_member("steps", &WiringRecord::steps));
        }
    };

    struct AudioSinkRecord {
        uint32_t channel {};
        std::string fn_name;

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("channel", &AudioSinkRecord::channel),
                IO::member("fn_name", &AudioSinkRecord::fn_name));
        }
    };

    struct RenderSinkRecord {
        std::string fn_name;

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("fn_name", &RenderSinkRecord::fn_name));
        }
    };

    struct EntityRecord {
        uint32_t id {};
        std::string kind;
        glm::vec3 position {};
        float intensity { 0.0F };
        float radius { 0.0F };
        float query_radius { 0.0F };
        std::optional<glm::vec3> color;
        std::optional<float> size;
        std::string influence_fn_name;
        std::string perception_fn_name;
        WiringRecord wiring;
        std::vector<AudioSinkRecord> audio_sinks;
        std::vector<RenderSinkRecord> render_sinks;

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("id", &EntityRecord::id),
                IO::member("kind", &EntityRecord::kind),
                IO::member("position", &EntityRecord::position),
                IO::member("intensity", &EntityRecord::intensity),
                IO::member("radius", &EntityRecord::radius),
                IO::member("query_radius", &EntityRecord::query_radius),
                IO::opt_member("color", &EntityRecord::color),
                IO::opt_member("size", &EntityRecord::size),
                IO::member("influence_fn_name", &EntityRecord::influence_fn_name),
                IO::member("perception_fn_name", &EntityRecord::perception_fn_name),
                IO::member("wiring", &EntityRecord::wiring),
                IO::member("audio_sinks", &EntityRecord::audio_sinks),
                IO::member("render_sinks", &EntityRecord::render_sinks));
        }
    };

    struct FabricSchema {
        uint32_t version { 4 };
        std::string fabric_name;
        std::vector<EntityRecord> entities;
        RangeSet ranges;

        static constexpr auto describe()
        {
            return std::make_tuple(
                IO::member("version", &FabricSchema::version),
                IO::member("fabric_name", &FabricSchema::fabric_name),
                IO::member("entities", &FabricSchema::entities),
                IO::member("ranges", &FabricSchema::ranges));
        }
    };

    // -------------------------------------------------------------------------
    // Range helpers
    // -------------------------------------------------------------------------

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

    float normalize(float value, const Range& r)
    {
        if (r.max <= r.min) {
            return 0.0F;
        }
        return (value - r.min) / (r.max - r.min);
    }

    // -------------------------------------------------------------------------
    // Wiring builder
    // -------------------------------------------------------------------------

    WiringRecord build_wiring(const Fabric& fabric, uint32_t id)
    {
        const Wiring* w = fabric.wiring_for(id);
        if (!w) {
            return { .kind = "unsupported" };
        }
        if (!w->move_steps().empty()) {
            std::vector<WiringStep> steps;
            steps.reserve(w->move_steps().size());
            for (const auto& s : w->move_steps()) {
                steps.push_back({ .position = s.position, .delay_seconds = s.delay_seconds });
            }
            WiringRecord rec { .kind = "move_to", .steps = std::move(steps) };
            if (w->times_count() > 1) {
                rec.times = w->times_count();
            }
            return rec;
        }
        if (w->interval().has_value()) {
            WiringRecord rec { .kind = "every", .interval = *w->interval() };
            if (w->duration().has_value()) {
                rec.duration = *w->duration();
            }
            if (w->times_count() > 1) {
                rec.times = w->times_count();
            }
            return rec;
        }
        return { .kind = "commit_driven" };
    }

    void fill_wiring_pixels(const Fabric& fabric, uint32_t id, float& trigger_out, float& time_out)
    {
        const Wiring* w = fabric.wiring_for(id);
        trigger_out = 0.0F;
        time_out = 0.0F;
        if (!w)
            return;
        if (!w->move_steps().empty()) {
            time_out = 1.0F;
        } else if (w->interval().has_value()) {
            trigger_out = 0.2F;
            if (w->duration().has_value())
                time_out = 0.5F;
        }
    }

} // namespace

bool StateEncoder::encode(const Fabric& fabric, const std::string& base_path)
{
    m_last_error.clear();

    // -------------------------------------------------------------------------
    // Collect encodable entities.
    // -------------------------------------------------------------------------
    struct InternalRecord {
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
        // Layer 0
        float entity_type_norm { 0.0F };
        float trigger_kind { 0.0F };
        float time_kind { 0.0F };
        // Layer 2
        uint32_t sink_type { 0 };
        uint32_t first_audio_channel { 0 };
    };

    std::vector<InternalRecord> records;

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
            auto& rec = records.emplace_back(InternalRecord {
                .id = id,
                .kind = k,
                .position = *e->position(),
                .intensity = e->intensity(),
                .radius = e->radius(),
                .color = e->color(),
                .size = e->size(),
                .influence_fn_name = e->fn_name(),
                .entity_type_norm = 0.0F,
            });
            fill_wiring_pixels(fabric, id, rec.trigger_kind, rec.time_kind);
            rec.sink_type = (e->audio_sinks().empty() ? 0U : 1U)
                          | (e->render_sinks().empty() ? 0U : 2U);
            if (!e->audio_sinks().empty())
                rec.first_audio_channel = e->audio_sinks().front().channel;
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
            auto& rec = records.emplace_back(InternalRecord {
                .id = id,
                .kind = k,
                .position = *s->position(),
                .query_radius = s->query_radius(),
                .perception_fn_name = s->fn_name(),
                .entity_type_norm = 0.333F,
            });
            fill_wiring_pixels(fabric, id, rec.trigger_kind, rec.time_kind);
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
            auto& rec = records.emplace_back(InternalRecord {
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
                .entity_type_norm = 0.667F,
            });
            fill_wiring_pixels(fabric, id, rec.trigger_kind, rec.time_kind);
            rec.sink_type = (a->audio_sinks().empty() ? 0U : 1U)
                          | (a->render_sinks().empty() ? 0U : 2U);
            if (!a->audio_sinks().empty())
                rec.first_audio_channel = a->audio_sinks().front().channel;
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
    // Compute per-field ranges.
    // -------------------------------------------------------------------------
    RangeSet rs;
    bool init_pos_x = false, init_pos_y = false, init_pos_z = false;
    bool init_intensity = false, init_radius = false, init_query_radius = false;
    bool init_color_r = false, init_color_g = false, init_color_b = false;
    bool init_size = false;

    for (const auto& rec : records) {
        expand_range(rs.pos_x, rec.position.x, init_pos_x);
        expand_range(rs.pos_y, rec.position.y, init_pos_y);
        expand_range(rs.pos_z, rec.position.z, init_pos_z);

        if (rec.kind == Fabric::Kind::Emitter || rec.kind == Fabric::Kind::Agent) {
            expand_range(rs.intensity, rec.intensity, init_intensity);
            expand_range(rs.radius, rec.radius, init_radius);
            if (rec.color) {
                expand_range(rs.color_r, rec.color->r, init_color_r);
                expand_range(rs.color_g, rec.color->g, init_color_g);
                expand_range(rs.color_b, rec.color->b, init_color_b);
            }
            if (rec.size) {
                expand_range(rs.size, *rec.size, init_size);
            }
        }
        if (rec.kind == Fabric::Kind::Sensor || rec.kind == Fabric::Kind::Agent) {
            expand_range(rs.query_radius, rec.query_radius, init_query_radius);
        }
    }

    // -------------------------------------------------------------------------
    // Build RGBA32F pixel buffer.
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

        const size_t row0 = (static_cast<size_t>(0) * width + i) * k_channels;
        pixels[row0 + 0] = normalize(rec.position.x, rs.pos_x);
        pixels[row0 + 1] = normalize(rec.position.y, rs.pos_y);
        pixels[row0 + 2] = normalize(rec.position.z, rs.pos_z);
        pixels[row0 + 3] = normalize(rec.intensity, rs.intensity);

        const size_t row1 = (static_cast<size_t>(1) * width + i) * k_channels;
        if (rec.color) {
            pixels[row1 + 0] = normalize(rec.color->r, rs.color_r);
            pixels[row1 + 1] = normalize(rec.color->g, rs.color_g);
            pixels[row1 + 2] = normalize(rec.color->b, rs.color_b);
        }
        if (rec.size) {
            pixels[row1 + 3] = normalize(*rec.size, rs.size);
        }

        const size_t row2 = (static_cast<size_t>(2) * width + i) * k_channels;
        pixels[row2 + 0] = normalize(rec.radius, rs.radius);
        pixels[row2 + 1] = normalize(rec.query_radius, rs.query_radius);

        const size_t row3 = (static_cast<size_t>(3) * width + i) * k_channels;
        pixels[row3 + 0] = static_cast<float>(rec.id);
        pixels[row3 + 1] = rec.entity_type_norm;
        pixels[row3 + 2] = rec.trigger_kind;
        pixels[row3 + 3] = rec.time_kind;

        const size_t row4 = (static_cast<size_t>(4) * width + i) * k_channels;
        pixels[row4 + 0] = static_cast<float>(rec.sink_type);
        pixels[row4 + 1] = static_cast<float>(rec.first_audio_channel);
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
    // Build and write schema.
    // -------------------------------------------------------------------------
    const char* kind_names[] = { "emitter", "sensor", "agent" };

    FabricSchema schema;
    schema.version = 4;
    schema.fabric_name = fabric.name();
    schema.ranges = rs;
    schema.entities.reserve(records.size());

    for (const auto& rec : records) {
        EntityRecord ent;
        ent.id = rec.id;
        ent.kind = kind_names[static_cast<int>(rec.kind)];
        ent.position = rec.position;
        ent.intensity = rec.intensity;
        ent.radius = rec.radius;
        ent.query_radius = rec.query_radius;
        ent.color = rec.color;
        ent.size = rec.size;
        ent.influence_fn_name = rec.influence_fn_name;
        ent.perception_fn_name = rec.perception_fn_name;
        ent.wiring = build_wiring(fabric, rec.id);

        if (rec.kind == Fabric::Kind::Emitter) {
            auto e = fabric.get_emitter(rec.id);
            for (const auto& s : e->audio_sinks())
                ent.audio_sinks.push_back({ s.channel, s.fn_name });
            for (const auto& s : e->render_sinks())
                ent.render_sinks.push_back({ s.fn_name });
        } else if (rec.kind == Fabric::Kind::Agent) {
            auto a = fabric.get_agent(rec.id);
            for (const auto& s : a->audio_sinks())
                ent.audio_sinks.push_back({ s.channel, s.fn_name });
            for (const auto& s : a->render_sinks())
                ent.render_sinks.push_back({ s.fn_name });
        }

        schema.entities.push_back(std::move(ent));
    }

    IO::JSONSerializer ser;
    const std::string json_path = base_path + ".json";
    if (!ser.write(json_path, schema)) {
        m_last_error = "Failed to write schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} entities to {} + {}",
        records.size(), exr_path, json_path);

    return true;
}

} // namespace MayaFlux::Nexus
