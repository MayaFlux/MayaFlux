#include "Encoder.hpp"

#include "MayaFlux/Nexus/Pheme/Sinks.hpp"

#include "MayaFlux/Nexus/Principals/Locus.hpp"
#include "MayaFlux/Nexus/Principals/Presence.hpp"

#include "MayaFlux/IO/ImageWriter.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    // -------------------------------------------------------------------------
    // State::Range helpers
    // -------------------------------------------------------------------------

    void expand_range(State::Range& r, float value, bool& initialized)
    {
        if (!initialized) {
            r.min = r.max = value;
            initialized = true;
        } else {
            r.min = std::min(r.min, value);
            r.max = std::max(r.max, value);
        }
    }

    float normalize(float value, const State::Range& r)
    {
        if (r.max <= r.min) {
            return 0.0F;
        }
        return (value - r.min) / (r.max - r.min);
    }

    // -------------------------------------------------------------------------
    // Wiring builder
    // -------------------------------------------------------------------------

    State::WiringRecord build_wiring(const Fabric& fabric, uint32_t id)
    {
        const Wiring* w = fabric.wiring_for(id);
        if (!w)
            return { .kind = State::WiringKind::Unsupported };

        if (!w->move_steps().empty()) {
            std::vector<State::WiringStep> steps;
            steps.reserve(w->move_steps().size());
            for (const auto& s : w->move_steps())
                steps.push_back({ .position = s.position, .delay_seconds = s.delay_seconds });
            State::WiringRecord rec { .kind = State::WiringKind::MoveTo, .steps = std::move(steps) };
            if (w->times_count() > 1)
                rec.times = w->times_count();

            return rec;
        }

        if (w->interval().has_value()) {
            State::WiringRecord rec { .kind = State::WiringKind::Every, .interval = w->interval() };
            rec.duration = w->duration();
            if (w->times_count() > 1)
                rec.times = w->times_count();

            return rec;
        }

        if (w->is_scroll())
            return { .kind = State::WiringKind::Scroll };

        return { .kind = State::WiringKind::CommitDriven };
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
    State::RangeSet rs;
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
    image.height = State::k_exr_rows;
    image.channels = State::k_channels;
    image.format = Portal::Graphics::ImageFormat::RGBA32F;

    std::vector<float> pixels(static_cast<size_t>(width) * State::k_exr_rows * State::k_channels, 0.0F);

    for (size_t i = 0; i < records.size(); ++i) {
        const auto& rec = records[i];

        const size_t row0 = (static_cast<size_t>(0) * width + i) * State::k_channels;
        pixels[row0 + 0] = normalize(rec.position.x, rs.pos_x);
        pixels[row0 + 1] = normalize(rec.position.y, rs.pos_y);
        pixels[row0 + 2] = normalize(rec.position.z, rs.pos_z);
        pixels[row0 + 3] = normalize(rec.intensity, rs.intensity);

        const size_t row1 = (static_cast<size_t>(1) * width + i) * State::k_channels;
        if (rec.color) {
            pixels[row1 + 0] = normalize(rec.color->r, rs.color_r);
            pixels[row1 + 1] = normalize(rec.color->g, rs.color_g);
            pixels[row1 + 2] = normalize(rec.color->b, rs.color_b);
        }
        if (rec.size) {
            pixels[row1 + 3] = normalize(*rec.size, rs.size);
        }

        const size_t row2 = (static_cast<size_t>(2) * width + i) * State::k_channels;
        pixels[row2 + 0] = normalize(rec.radius, rs.radius);
        pixels[row2 + 1] = normalize(rec.query_radius, rs.query_radius);

        const size_t row3 = (static_cast<size_t>(3) * width + i) * State::k_channels;
        pixels[row3 + 0] = static_cast<float>(rec.id);
        pixels[row3 + 1] = rec.entity_type_norm;
        pixels[row3 + 2] = rec.trigger_kind;
        pixels[row3 + 3] = rec.time_kind;

        const size_t row4 = (static_cast<size_t>(4) * width + i) * State::k_channels;
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
    State::FabricSchema schema;
    schema.version = State::k_schema_version;
    schema.fabric_name = fabric.name();
    schema.ranges = rs;
    schema.entities.reserve(records.size());

    for (const auto& rec : records) {
        State::EntityRecord ent;
        ent.id = rec.id;
        ent.kind = State::kind_to_string(rec.kind);
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
                ent.audio_sinks.push_back({ .channel = s.channel, .fn_name = s.fn_name });
            for (const auto& s : e->render_sinks())
                ent.render_sinks.push_back({ .fn_name = s.fn_name });
        } else if (rec.kind == Fabric::Kind::Agent) {
            auto a = fabric.get_agent(rec.id);
            for (const auto& s : a->audio_sinks())
                ent.audio_sinks.push_back({ .channel = s.channel, .fn_name = s.fn_name });

            for (const auto& s : a->render_sinks())
                ent.render_sinks.push_back({ .fn_name = s.fn_name });

            if (auto locus = std::dynamic_pointer_cast<Locus>(a)) {
                ent.subkind = "locus";
                const auto& nav = locus->nav();
                ent.locus_nav = State::LocusNavRecord {
                    .eye = nav.eye,
                    .target = nav.eye + glm::vec3 { std::cos(nav.pitch) * std::sin(nav.yaw), std::sin(nav.pitch), std::cos(nav.pitch) * std::cos(nav.yaw) },
                    .up = { 0.0F, 1.0F, 0.0F },
                    .fov = nav.fov_radians,
                    .near_plane = nav.near_plane,
                    .far_plane = nav.far_plane,
                    .speed = nav.move_speed,
                };
            } else if (auto presence = std::dynamic_pointer_cast<Presence>(a)) {
                ent.subkind = "presence";
                ent.radiate_fn_name = presence->radiate_fn_name();
                ent.falloff_radius = presence->falloff_radius() != presence->query_radius()
                    ? std::optional<float>(presence->falloff_radius())
                    : std::nullopt;
                if (auto fc = presence->falloff_curve())
                    ent.falloff_curve_name = Reflect::enum_to_lowercase_string(*fc);
            }
        }

        schema.entities.push_back(std::move(ent));
    }

    for (uint32_t xid : fabric.all_expanse_ids()) {
        const auto x = fabric.get_expanse(xid);
        if (!x)
            continue;
        if (x->fn_name().empty()) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: Expanse {} has no fn_name, skipping", xid);
            continue;
        }
        schema.expanses.push_back(State::ExpanseRecord {
            .id = xid,
            .fn_name = x->fn_name(),
            .on_enter_fn_name = x->on_enter_fn_name(),
            .on_exit_fn_name = x->on_exit_fn_name(),
        });
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

bool StateEncoder::encode(const Tapestry& tapestry, const std::string& base_dir, nlohmann::json user_state)
{
    m_last_error.clear();

    State::TapestrySchema schema;

    for (const auto& fabric : tapestry.all_fabrics()) {
        const std::string fabric_id = fabric->name().empty()
            ? std::to_string(fabric->id())
            : fabric->name();

        const std::string base_path = base_dir + "/" + fabric_id;

        if (!encode(*fabric, base_path)) {
            return false;
        }

        schema.fabrics.push_back(State::FabricRef {
            .name = fabric_id,
            .base_path = base_path,
        });
    }

    for (const auto& [xname, xptr] : tapestry.all_expanses()) {
        State::TapestryExpanseRecord xrec {
            .name = xname,
            .fn_name = xptr->fn_name(),
            .on_enter_fn_name = xptr->on_enter_fn_name(),
            .on_exit_fn_name = xptr->on_exit_fn_name(),
        };
        for (const auto& fabric : tapestry.all_fabrics()) {
            for (uint32_t xid : fabric->all_expanse_ids()) {
                if (fabric->get_expanse(xid) == xptr) {
                    const std::string fname = fabric->name().empty()
                        ? std::to_string(fabric->id())
                        : fabric->name();
                    xrec.fabric_names.push_back(fname);
                    break;
                }
            }
        }
        schema.expanses.push_back(std::move(xrec));
    }

    schema.user_state = std::move(user_state);

    IO::JSONSerializer ser;
    const std::string tapestry_path = base_dir + "/tapestry.json";
    if (!ser.write(tapestry_path, schema)) {
        m_last_error = "Failed to write tapestry schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} fabrics to {}", schema.fabrics.size(), tapestry_path);
    return true;
}

} // namespace MayaFlux::Nexus
