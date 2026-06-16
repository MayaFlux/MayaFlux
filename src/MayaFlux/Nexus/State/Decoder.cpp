#include "Decoder.hpp"

#include "MayaFlux/Nexus/Principals/Locus.hpp"

#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    float denormalize(float norm, const State::Range& r)
    {
        return r.min + norm * (r.max - r.min);
    }

    void apply_wiring(Wiring wiring, const State::WiringRecord& rec, std::vector<std::string>& warnings)
    {
        switch (rec.kind) {
        case State::WiringKind::Every:
            wiring.every(*rec.interval);
            if (rec.duration)
                wiring.for_duration(*rec.duration);
            if (rec.times && *rec.times > 1)
                wiring.times(*rec.times);
            break;

        case State::WiringKind::MoveTo:
            if (rec.steps) {
                for (const auto& s : *rec.steps)
                    wiring.move_to(s.position, s.delay_seconds);
            }
            if (rec.times && *rec.times > 1)
                wiring.times(*rec.times);
            break;

        case State::WiringKind::Scroll:
            warnings.emplace_back("Scroll wiring cannot be reconstructed without a live window; falling back to commit_driven");
            [[fallthrough]];

        case State::WiringKind::Unsupported:
            warnings.emplace_back("Unsupported wiring kind in schema; falling back to commit_driven");
            [[fallthrough]];

        case State::WiringKind::CommitDriven:
            break;
        }
        wiring.finalise();
    }

    // -------------------------------------------------------------------------
    // EXR load + validation, shared by decode() and reconstruct()
    // -------------------------------------------------------------------------

    struct PixelView {
        IO::ImageData image;
        const std::vector<float>* pixels { nullptr };
        uint32_t width { 0 };
    };

    std::optional<PixelView> load_exr(
        const std::string& exr_path,
        uint32_t expected_entity_count,
        uint32_t expected_rows,
        std::string& error_out)
    {
        auto image_opt = IO::ImageReader::load(exr_path, 0);
        if (!image_opt) {
            error_out = "Failed to load EXR: " + exr_path;
            return std::nullopt;
        }

        const auto* pixels = image_opt->as_float();
        if (!pixels || pixels->empty()) {
            error_out = "EXR has no float pixel data: " + exr_path;
            return std::nullopt;
        }
        if (image_opt->channels != State::k_channels) {
            error_out = "EXR channel count mismatch: expected "
                + std::to_string(State::k_channels) + " got " + std::to_string(image_opt->channels);
            return std::nullopt;
        }
        if (image_opt->height != expected_rows) {
            error_out = "EXR row count mismatch: expected "
                + std::to_string(expected_rows) + " got " + std::to_string(image_opt->height);
            return std::nullopt;
        }
        if (image_opt->width != expected_entity_count) {
            error_out = "EXR width (" + std::to_string(image_opt->width)
                + ") does not match entity count (" + std::to_string(expected_entity_count) + ")";
            return std::nullopt;
        }

        const uint32_t w = image_opt->width;
        return PixelView { .image = std::move(*image_opt), .pixels = nullptr, .width = w };
    }

    // =========================================================================
    // Helper used by both decode() and reconstruct() to apply locus_nav to a
    // live Locus. Returns true if the cast succeeded.
    // =========================================================================

    bool apply_locus_nav(const std::shared_ptr<Agent>& a, const State::LocusNavRecord& nav)
    {
        auto locus = std::dynamic_pointer_cast<Locus>(a);
        if (!locus)
            return false;
        locus->nav().eye = nav.eye;
        locus->nav().fov_radians = nav.fov;
        locus->nav().near_plane = nav.near_plane;
        locus->nav().far_plane = nav.far_plane;
        locus->nav().move_speed = nav.speed;
        // Derive yaw/pitch from the stored target direction.
        const glm::vec3 dir = glm::normalize(nav.target - nav.eye);
        locus->nav().yaw = std::atan2(dir.x, dir.z);
        locus->nav().pitch = std::asin(glm::clamp(dir.y, -1.0F, 1.0F));
        return true;
    }

} // namespace

// -------------------------------------------------------------------------
// decode()
// -------------------------------------------------------------------------

bool StateDecoder::decode(Fabric& fabric, const std::string& base_path)
{
    m_last_error.clear();
    m_patched_count = 0;
    m_missing_count = 0;

    const std::string json_path = base_path + ".json";
    const std::string exr_path = base_path + ".exr";

    IO::JSONSerializer ser;
    auto schema_opt = ser.read<State::FabricSchema>(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    const auto& schema = *schema_opt;

    if (schema.version != State::k_schema_version) {
        m_last_error = "Unsupported schema version: " + std::to_string(schema.version)
            + " (expected " + std::to_string(State::k_schema_version) + ")";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (schema.entities.empty()) {
        m_last_error = "Schema contains no entities: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const uint32_t expected_rows = State::k_exr_rows;
    auto pv_opt = load_exr(exr_path, static_cast<uint32_t>(schema.entities.size()), expected_rows, m_last_error);
    if (!pv_opt) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    auto& pv = *pv_opt;
    const auto* pixels = pv.image.as_float();
    const uint32_t width = pv.width;
    const auto& r = schema.ranges;

    for (size_t i = 0; i < schema.entities.size(); ++i) {
        const auto& entry = schema.entities[i];

        if (!State::kind_known(entry.kind)) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                "StateDecoder: unknown kind '{}' for id {}, skipping", entry.kind, entry.id);
            ++m_missing_count;
            continue;
        }

        const size_t row0 = (static_cast<size_t>(0) * width + i) * State::k_channels;
        const size_t row1 = (static_cast<size_t>(1) * width + i) * State::k_channels;
        const size_t row2 = (static_cast<size_t>(2) * width + i) * State::k_channels;

        const glm::vec3 position {
            denormalize((*pixels)[row0 + 0], r.pos_x),
            denormalize((*pixels)[row0 + 1], r.pos_y),
            denormalize((*pixels)[row0 + 2], r.pos_z),
        };

        switch (State::parse_kind(entry.kind)) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(entry.id);
            if (!e) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: id {} not found as Emitter, skipping", entry.id);
                ++m_missing_count;
                continue;
            }
            if (!entry.influence_fn_name.empty() && e->fn_name() != entry.influence_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Emitter {} fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.influence_fn_name, e->fn_name());
            }
            e->set_position(position);
            e->set_intensity(denormalize((*pixels)[row0 + 3], r.intensity));
            if (entry.color) {
                e->set_color(glm::vec3 {
                    denormalize((*pixels)[row1 + 0], r.color_r),
                    denormalize((*pixels)[row1 + 1], r.color_g),
                    denormalize((*pixels)[row1 + 2], r.color_b),
                });
            }
            if (entry.size) {
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
            if (!entry.perception_fn_name.empty() && s->fn_name() != entry.perception_fn_name) {
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
            if (!entry.perception_fn_name.empty() && a->perception_fn_name() != entry.perception_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Agent {} perception_fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.perception_fn_name, a->perception_fn_name());
            }
            if (!entry.influence_fn_name.empty() && a->influence_fn_name() != entry.influence_fn_name) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                    "StateDecoder: Agent {} influence_fn_name mismatch: schema='{}' live='{}'",
                    entry.id, entry.influence_fn_name, a->influence_fn_name());
            }
            a->set_position(position);
            a->set_intensity(denormalize((*pixels)[row0 + 3], r.intensity));
            if (entry.color) {
                a->set_color(glm::vec3 {
                    denormalize((*pixels)[row1 + 0], r.color_r),
                    denormalize((*pixels)[row1 + 1], r.color_g),
                    denormalize((*pixels)[row1 + 2], r.color_b),
                });
            }
            if (entry.size) {
                a->set_size(denormalize((*pixels)[row1 + 3], r.size));
            }
            a->set_radius(denormalize((*pixels)[row2 + 0], r.radius));
            a->set_query_radius(denormalize((*pixels)[row2 + 1], r.query_radius));

            if (entry.locus_nav) {
                if (!apply_locus_nav(a, *entry.locus_nav)) {
                    MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                        "StateDecoder: Agent {} has locus_nav in schema but is not a Locus at runtime",
                        entry.id);
                }
            }

            if (auto presence = std::dynamic_pointer_cast<Presence>(a)) {
                if (!entry.falloff_curve_name.empty()) {
                    if (auto fc = Reflect::string_to_enum_case_insensitive<Presence::FalloffCurve>(entry.falloff_curve_name))
                        presence->set_falloff_curve(*fc);
                }
                if (entry.falloff_radius)
                    presence->set_falloff_radius(*entry.falloff_radius);
            }
            break;
        }
        }

        ++m_patched_count;
    }

    for (const auto& xrec : schema.expanses) {
        if (xrec.fn_name.empty()) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateDecoder: Expanse {} has no fn_name, skipping", xrec.id);
            continue;
        }
        auto contains_fn = fabric.resolve_expanse_fn(xrec.fn_name);
        if (!contains_fn || !*contains_fn) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateDecoder: Expanse {} fn '{}' not in registry, skipping",
                xrec.id, xrec.fn_name);
            continue;
        }
        auto on_enter_fn = xrec.on_enter_fn_name.empty()
            ? Expanse::CrossingFn {}
            : [ptr = fabric.resolve_crossing_fn(xrec.on_enter_fn_name)](uint32_t id) {
                  if (ptr && *ptr)
                      (*ptr)(id);
              };
        auto on_exit_fn = xrec.on_exit_fn_name.empty()
            ? Expanse::CrossingFn {}
            : [ptr = fabric.resolve_crossing_fn(xrec.on_exit_fn_name)](uint32_t id) {
                  if (ptr && *ptr)
                      (*ptr)(id);
              };
        auto expanse = std::make_shared<Expanse>(
            xrec.fn_name,
            xrec.on_enter_fn_name,
            xrec.on_exit_fn_name,
            *contains_fn,
            std::move(on_enter_fn),
            std::move(on_exit_fn));
        fabric.add_expanse(std::move(expanse));
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder: patched {} entities ({} missing) from {} + {}",
        m_patched_count, m_missing_count, exr_path, json_path);

    return true;
}

// -------------------------------------------------------------------------
// reconstruct()
// -------------------------------------------------------------------------

StateDecoder::ReconstructionResult StateDecoder::reconstruct(Fabric& fabric, const std::string& base_path)
{
    ReconstructionResult result;
    m_last_error.clear();

    const std::string json_path = base_path + ".json";
    const std::string exr_path = base_path + ".exr";

    IO::JSONSerializer ser;
    auto schema_opt = ser.read<State::FabricSchema>(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    const auto& schema = *schema_opt;

    if (schema.version != State::k_schema_version) {
        m_last_error = "Unsupported schema version: " + std::to_string(schema.version)
            + " (expected " + std::to_string(State::k_schema_version) + ")";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }

    if (schema.entities.empty()) {
        m_last_error = "Schema contains no entities: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }

    const uint32_t expected_rows = State::k_exr_rows;
    auto pv_opt = load_exr(exr_path, static_cast<uint32_t>(schema.entities.size()), expected_rows, m_last_error);
    if (!pv_opt) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    auto& pv = *pv_opt;
    const auto* pixels = pv.image.as_float();
    const uint32_t width = pv.width;
    const auto& r = schema.ranges;

    const auto existing_ids = fabric.all_ids();
    const std::unordered_set<uint32_t> existing(existing_ids.begin(), existing_ids.end());

    for (size_t i = 0; i < schema.entities.size(); ++i) {
        const auto& entry = schema.entities[i];

        if (!State::kind_known(entry.kind)) {
            result.warnings.push_back("Unknown kind '" + entry.kind
                + "' for id " + std::to_string(entry.id) + ", skipping");
            ++result.skipped;
            continue;
        }

        const size_t row0 = (static_cast<size_t>(0) * width + i) * State::k_channels;
        const size_t row1 = (static_cast<size_t>(1) * width + i) * State::k_channels;
        const size_t row2 = (static_cast<size_t>(2) * width + i) * State::k_channels;

        const glm::vec3 position {
            denormalize((*pixels)[row0 + 0], r.pos_x),
            denormalize((*pixels)[row0 + 1], r.pos_y),
            denormalize((*pixels)[row0 + 2], r.pos_z),
        };
        const float intensity = denormalize((*pixels)[row0 + 3], r.intensity);
        const float radius = denormalize((*pixels)[row2 + 0], r.radius);
        const float query_radius = denormalize((*pixels)[row2 + 1], r.query_radius);

        auto read_color = [&]() -> glm::vec3 {
            return {
                denormalize((*pixels)[row1 + 0], r.color_r),
                denormalize((*pixels)[row1 + 1], r.color_g),
                denormalize((*pixels)[row1 + 2], r.color_b),
            };
        };
        auto read_size = [&]() {
            return denormalize((*pixels)[row1 + 3], r.size);
        };

        if (existing.count(entry.id)) {
            // -----------------------------------------------------------------
            // Patch existing entity.
            // -----------------------------------------------------------------
            switch (State::parse_kind(entry.kind)) {
            case Fabric::Kind::Emitter: {
                auto e = fabric.get_emitter(entry.id);
                if (!e) {
                    ++result.skipped;
                    continue;
                }
                if (!entry.influence_fn_name.empty() && e->fn_name() != entry.influence_fn_name) {
                    result.warnings.push_back("Emitter " + std::to_string(entry.id)
                        + " fn_name mismatch: schema='" + entry.influence_fn_name
                        + "' live='" + e->fn_name() + "'");
                }
                e->set_position(position);
                e->set_intensity(intensity);
                if (entry.color) {
                    e->set_color(read_color());
                }
                if (entry.size) {
                    e->set_size(read_size());
                }
                e->set_radius(radius);
                break;
            }
            case Fabric::Kind::Sensor: {
                auto s = fabric.get_sensor(entry.id);
                if (!s) {
                    ++result.skipped;
                    continue;
                }
                if (!entry.perception_fn_name.empty() && s->fn_name() != entry.perception_fn_name) {
                    result.warnings.push_back("Sensor " + std::to_string(entry.id)
                        + " fn_name mismatch: schema='" + entry.perception_fn_name
                        + "' live='" + s->fn_name() + "'");
                }
                s->set_position(position);
                s->set_query_radius(query_radius);
                break;
            }
            case Fabric::Kind::Agent: {
                auto a = fabric.get_agent(entry.id);
                if (!a) {
                    ++result.skipped;
                    continue;
                }
                if (!entry.perception_fn_name.empty() && a->perception_fn_name() != entry.perception_fn_name) {
                    result.warnings.push_back("Agent " + std::to_string(entry.id)
                        + " perception_fn mismatch: schema='" + entry.perception_fn_name + "'");
                }
                if (!entry.influence_fn_name.empty() && a->influence_fn_name() != entry.influence_fn_name) {
                    result.warnings.push_back("Agent " + std::to_string(entry.id)
                        + " influence_fn mismatch: schema='" + entry.influence_fn_name + "'");
                }
                a->set_position(position);
                a->set_intensity(intensity);
                if (entry.color) {
                    a->set_color(read_color());
                }
                if (entry.size) {
                    a->set_size(read_size());
                }
                a->set_radius(radius);
                a->set_query_radius(query_radius);
                break;
            }
            }
            ++result.patched;

        } else {
            // -----------------------------------------------------------------
            // Construct missing entity.
            // -----------------------------------------------------------------
            switch (State::parse_kind(entry.kind)) {
            case Fabric::Kind::Emitter: {
                auto fn_ptr = fabric.resolve_influence_fn(entry.influence_fn_name);
                Emitter::InfluenceFn fn;
                if (!fn_ptr || !*fn_ptr) {
                    result.warnings.push_back("Emitter: unknown influence_fn '"
                        + entry.influence_fn_name + "', using no-op");
                    fn = [](const InfluenceContext&) { };
                } else {
                    fn = *fn_ptr;
                }
                auto emitter = std::make_shared<Emitter>(entry.influence_fn_name, std::move(fn));
                emitter->set_position(position);
                emitter->set_intensity(intensity);
                emitter->set_radius(radius);
                if (entry.color) {
                    emitter->set_color(read_color());
                }
                if (entry.size) {
                    emitter->set_size(read_size());
                }
                auto wiring = fabric.wire(emitter);
                if (emitter->id() != entry.id) {
                    result.warnings.push_back("Emitter schema_id=" + std::to_string(entry.id)
                        + " reconstructed as runtime_id=" + std::to_string(emitter->id()));
                }
                apply_wiring(std::move(wiring), entry.wiring, result.warnings);
                break;
            }
            case Fabric::Kind::Sensor: {
                auto fn_ptr = fabric.resolve_perception_fn(entry.perception_fn_name);
                Sensor::PerceptionFn fn;
                if (!fn_ptr || !*fn_ptr) {
                    result.warnings.push_back("Sensor: unknown perception_fn '"
                        + entry.perception_fn_name + "', using no-op");
                    fn = [](const PerceptionContext&) { };
                } else {
                    fn = *fn_ptr;
                }
                auto sensor = std::make_shared<Sensor>(query_radius,
                    entry.perception_fn_name, std::move(fn));
                sensor->set_position(position);
                auto wiring = fabric.wire(sensor);
                if (sensor->id() != entry.id) {
                    result.warnings.push_back("Sensor schema_id=" + std::to_string(entry.id)
                        + " reconstructed as runtime_id=" + std::to_string(sensor->id()));
                }
                apply_wiring(std::move(wiring), entry.wiring, result.warnings);
                break;
            }
            case Fabric::Kind::Agent: {
                auto pfn_ptr = fabric.resolve_perception_fn(entry.perception_fn_name);
                Agent::PerceptionFn pfn;
                if (!pfn_ptr || !*pfn_ptr) {
                    result.warnings.push_back("Agent: unknown perception_fn '"
                        + entry.perception_fn_name + "', using no-op");
                    pfn = [](const PerceptionContext&) { };
                } else {
                    pfn = *pfn_ptr;
                }
                auto ifn_ptr = fabric.resolve_influence_fn(entry.influence_fn_name);
                Agent::InfluenceFn ifn;
                if (!ifn_ptr || !*ifn_ptr) {
                    result.warnings.push_back("Agent: unknown influence_fn '"
                        + entry.influence_fn_name + "', using no-op");
                    ifn = [](const InfluenceContext&) { };
                } else {
                    ifn = *ifn_ptr;
                }
                std::shared_ptr<Agent> agent;
                if (entry.subkind == "locus" && entry.locus_nav) {
                    const auto& nav = *entry.locus_nav;
                    Kinesis::NavigationConfig cfg {
                        .initial_eye = nav.eye,
                        .initial_target = nav.target,
                        .fov_radians = nav.fov,
                        .near_plane = nav.near_plane,
                        .far_plane = nav.far_plane,
                        .move_speed = nav.speed,
                    };
                    agent = std::make_shared<Locus>(cfg, query_radius,
                        entry.perception_fn_name, std::move(pfn),
                        entry.influence_fn_name, std::move(ifn));
                    result.warnings.push_back("Locus " + std::to_string(entry.id)
                        + ": view_targets must be reconnected by caller");

                } else if (entry.subkind == "presence") {
                    auto rfn_ptr = fabric.resolve_radiate_fn(entry.radiate_fn_name);
                    Presence::RadiateFn rfn;
                    if (!rfn_ptr || !*rfn_ptr) {
                        result.warnings.push_back("Presence: unknown radiate_fn '"
                            + entry.radiate_fn_name + "', using no-op");
                        rfn = [](uint32_t, float) { };
                    } else {
                        rfn = *rfn_ptr;
                    }
                    auto presence = std::make_shared<Presence>(query_radius,
                        entry.perception_fn_name, std::move(pfn),
                        entry.influence_fn_name, std::move(ifn),
                        entry.radiate_fn_name, std::move(rfn));
                    if (!entry.falloff_curve_name.empty()) {
                        if (auto fc = Reflect::string_to_enum_case_insensitive<Presence::FalloffCurve>(entry.falloff_curve_name))
                            presence->set_falloff_curve(*fc);
                    }
                    if (entry.falloff_radius)
                        presence->set_falloff_radius(*entry.falloff_radius);
                    agent = std::move(presence);

                } else {
                    if (entry.subkind == "locus") {
                        result.warnings.push_back("Locus " + std::to_string(entry.id)
                            + ": no locus_nav in schema, reconstructed as plain Agent");
                    }
                    agent = std::make_shared<Agent>(query_radius,
                        entry.perception_fn_name, std::move(pfn),
                        entry.influence_fn_name, std::move(ifn));
                }
                agent->set_position(position);
                agent->set_intensity(intensity);
                agent->set_radius(radius);
                agent->set_query_radius(query_radius);
                if (entry.color) {
                    agent->set_color(read_color());
                }
                if (entry.size) {
                    agent->set_size(read_size());
                }
                auto wiring = fabric.wire(agent);
                if (agent->id() != entry.id) {
                    result.warnings.push_back("Agent schema_id=" + std::to_string(entry.id)
                        + " reconstructed as runtime_id=" + std::to_string(agent->id()));
                }
                apply_wiring(std::move(wiring), entry.wiring, result.warnings);
                break;
            }
            }
            ++result.constructed;
        }
    }

    for (const auto& xrec : schema.expanses) {
        if (xrec.fn_name.empty()) {
            result.warnings.push_back("Expanse " + std::to_string(xrec.id)
                + ": no fn_name, skipping");
            continue;
        }
        auto contains_fn = fabric.resolve_expanse_fn(xrec.fn_name);
        if (!contains_fn || !*contains_fn) {
            result.warnings.push_back("Expanse " + std::to_string(xrec.id)
                + ": fn '" + xrec.fn_name + "' not in registry, skipping");
            continue;
        }
        auto on_enter_fn = xrec.on_enter_fn_name.empty()
            ? Expanse::CrossingFn {}
            : [ptr = fabric.resolve_crossing_fn(xrec.on_enter_fn_name)](uint32_t id) {
                  if (ptr && *ptr)
                      (*ptr)(id);
              };
        auto on_exit_fn = xrec.on_exit_fn_name.empty()
            ? Expanse::CrossingFn {}
            : [ptr = fabric.resolve_crossing_fn(xrec.on_exit_fn_name)](uint32_t id) {
                  if (ptr && *ptr)
                      (*ptr)(id);
              };
        auto expanse = std::make_shared<Expanse>(
            xrec.fn_name,
            xrec.on_enter_fn_name,
            xrec.on_exit_fn_name,
            *contains_fn,
            std::move(on_enter_fn),
            std::move(on_exit_fn));
        fabric.add_expanse(std::move(expanse));
        ++result.constructed;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder::reconstruct: constructed={} patched={} skipped={} warnings={}",
        result.constructed, result.patched, result.skipped, result.warnings.size());

    return result;
}

StateDecoder::ReconstructionResult StateDecoder::reconstruct(
    Tapestry& tapestry, const std::string& base_dir)
{
    ReconstructionResult total;

    const std::string tapestry_path = base_dir + "/tapestry.json";
    IO::JSONSerializer ser;
    auto schema_opt = ser.read<State::TapestrySchema>(tapestry_path);
    if (!schema_opt) {
        m_last_error = "Failed to load tapestry schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return total;
    }
    const auto& schema = *schema_opt;

    for (const auto& ref : schema.fabrics) {
        auto fabric = tapestry.get_fabric(ref.name);
        if (!fabric) {
            fabric = tapestry.create_fabric(ref.name);
        }
        auto result = reconstruct(*fabric, ref.base_path);
        total.constructed += result.constructed;
        total.patched += result.patched;
        total.skipped += result.skipped;
        for (auto& w : result.warnings)
            total.warnings.push_back(ref.name + ": " + std::move(w));
    }

    for (const auto& xrec : schema.expanses) {
        if (xrec.fn_name.empty()) {
            total.warnings.push_back("TapestryExpanse '" + xrec.name + "': no fn_name, skipping");
            continue;
        }
        Expanse::ContainsFn contains_fn;
        Expanse::CrossingFn on_enter_fn;
        Expanse::CrossingFn on_exit_fn;

        for (const auto& fname : xrec.fabric_names) {
            auto fabric = tapestry.get_fabric(fname);
            if (!fabric)
                continue;
            if (!contains_fn) {
                if (auto ptr = fabric->resolve_expanse_fn(xrec.fn_name); ptr && *ptr)
                    contains_fn = *ptr;
            }
            if (!on_enter_fn && !xrec.on_enter_fn_name.empty()) {
                if (auto ptr = fabric->resolve_crossing_fn(xrec.on_enter_fn_name); ptr && *ptr)
                    on_enter_fn = *ptr;
            }
            if (!on_exit_fn && !xrec.on_exit_fn_name.empty()) {
                if (auto ptr = fabric->resolve_crossing_fn(xrec.on_exit_fn_name); ptr && *ptr)
                    on_exit_fn = *ptr;
            }
        }

        if (!contains_fn) {
            total.warnings.push_back("TapestryExpanse '" + xrec.name
                + "': fn '" + xrec.fn_name + "' not resolved, skipping");
            continue;
        }

        auto expanse = tapestry.create_expanse(
            xrec.name,
            std::move(contains_fn),
            std::move(on_enter_fn),
            std::move(on_exit_fn));

        for (const auto& fname : xrec.fabric_names) {
            if (auto fabric = tapestry.get_fabric(fname))
                fabric->add_expanse(expanse);
        }
        ++total.constructed;
    }

    total.user_state = schema.user_state;

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder::reconstruct(Tapestry): constructed={} patched={} skipped={} warnings={}",
        total.constructed, total.patched, total.skipped, total.warnings.size());
    return total;
}

} // namespace MayaFlux::Nexus
