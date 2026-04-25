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

    enum class WiringKind : uint8_t { CommitDriven,
        Every,
        MoveTo,
        Unsupported };

    struct WiringConfig {
        struct Step {
            glm::vec3 position {};
            double delay_seconds { 0.0 };
        };
        WiringKind kind { WiringKind::CommitDriven };
        std::optional<double> interval;
        std::optional<double> duration;
        size_t times { 1 };
        std::vector<Step> steps;
    };

    struct EntityEntry {
        uint32_t id;
        Fabric::Kind kind;
        std::string influence_fn_name;
        std::string perception_fn_name;
        bool has_color { false };
        bool has_size { false };
        WiringConfig wiring;
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
        return { .min = r.at("min").get<float>(), .max = r.at("max").get<float>() };
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
            const auto version = doc.at("version").get<uint32_t>();
            if (version != 2 && version != 3) {
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

                if (ent.contains("wiring")) {
                    const auto& w = ent.at("wiring");
                    const auto wk = w.at("kind").get<std::string>();
                    if (wk == "every") {
                        entry.wiring.kind = WiringKind::Every;
                        entry.wiring.interval = w.at("interval").get<double>();
                        if (w.contains("duration")) {
                            entry.wiring.duration = w.at("duration").get<double>();
                        }
                        entry.wiring.times = w.value("times", size_t { 1 });
                    } else if (wk == "move_to") {
                        entry.wiring.kind = WiringKind::MoveTo;
                        entry.wiring.times = w.value("times", size_t { 1 });
                        for (const auto& s : w.at("steps")) {
                            entry.wiring.steps.push_back({
                                .position = { s.at("x").get<float>(), s.at("y").get<float>(), s.at("z").get<float>() },
                                .delay_seconds = s.at("delay").get<double>(),
                            });
                        }
                    } else if (wk == "unsupported") {
                        entry.wiring.kind = WiringKind::Unsupported;
                    }
                }

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

    void apply_wiring(Wiring wiring, const WiringConfig& cfg, std::vector<std::string>& warnings)
    {
        switch (cfg.kind) {
        case WiringKind::Every:
            wiring.every(*cfg.interval);
            if (cfg.duration) {
                wiring.for_duration(*cfg.duration);
            }
            if (cfg.times > 1) {
                wiring.times(cfg.times);
            }
            wiring.finalise();
            break;
        case WiringKind::MoveTo:
            for (const auto& step : cfg.steps) {
                wiring.move_to(step.position, step.delay_seconds);
            }
            if (cfg.times > 1) {
                wiring.times(cfg.times);
            }
            wiring.finalise();
            break;
        case WiringKind::Unsupported:
            warnings.emplace_back("Unsupported wiring kind; entity registered as commit_driven");
            wiring.finalise();
            break;
        case WiringKind::CommitDriven:
        default:
            wiring.finalise();
            break;
        }
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

        const size_t row0 = (static_cast<size_t>(0 * width) + i) * k_channels;
        const size_t row1 = (static_cast<size_t>(1 * width) + i) * k_channels;
        const size_t row2 = (static_cast<size_t>(2 * width) + i) * k_channels;

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

StateDecoder::ReconstructionResult StateDecoder::reconstruct(Fabric& fabric, const std::string& base_path)
{
    ReconstructionResult result;
    m_last_error.clear();

    const std::string json_path = base_path + ".json";
    const std::string exr_path = base_path + ".exr";

    auto schema_opt = load_schema(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    const auto& schema = *schema_opt;

    auto image_opt = IO::ImageReader::load(exr_path, 0);
    if (!image_opt) {
        m_last_error = "Failed to load EXR: " + exr_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    const auto& image = *image_opt;

    const auto* pixels = image.as_float();
    if (!pixels || pixels->empty()) {
        m_last_error = "EXR has no float pixel data: " + exr_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    if (image.channels != k_channels) {
        m_last_error = "EXR channel count mismatch: got " + std::to_string(image.channels);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    if (image.height != k_exr_rows) {
        m_last_error = "EXR row count mismatch: got " + std::to_string(image.height);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    if (image.width != static_cast<uint32_t>(schema.entities.size())) {
        m_last_error = "EXR width (" + std::to_string(image.width) + ") != entity count ("
            + std::to_string(schema.entities.size()) + ")";
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }

    const uint32_t width = image.width;
    const auto& r = schema.ranges;

    const auto existing_ids = fabric.all_ids();
    const std::unordered_set<uint32_t> existing(existing_ids.begin(), existing_ids.end());

    for (size_t i = 0; i < schema.entities.size(); ++i) {
        const auto& entry = schema.entities[i];

        const size_t row0 = (static_cast<size_t>(0 * width) + i) * k_channels;
        const size_t row1 = (static_cast<size_t>(1 * width) + i) * k_channels;
        const size_t row2 = (static_cast<size_t>(2 * width) + i) * k_channels;

        const glm::vec3 position {
            denormalize((*pixels)[row0 + 0], r.pos_x),
            denormalize((*pixels)[row0 + 1], r.pos_y),
            denormalize((*pixels)[row0 + 2], r.pos_z),
        };
        const float intensity = denormalize((*pixels)[row0 + 3], r.intensity);
        const float radius = denormalize((*pixels)[row2 + 0], r.radius);
        const float query_radius = denormalize((*pixels)[row2 + 1], r.query_radius);

        if (existing.count(entry.id)) {
            // ----------------------------------------------------------------
            // Patch existing entity — same logic as decode().
            // ----------------------------------------------------------------
            switch (entry.kind) {
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
                if (entry.has_color) {
                    e->set_color({ denormalize((*pixels)[row1 + 0], r.color_r),
                        denormalize((*pixels)[row1 + 1], r.color_g),
                        denormalize((*pixels)[row1 + 2], r.color_b) });
                }
                if (entry.has_size) {
                    e->set_size(denormalize((*pixels)[row1 + 3], r.size));
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
                if (entry.has_color) {
                    a->set_color({ denormalize((*pixels)[row1 + 0], r.color_r),
                        denormalize((*pixels)[row1 + 1], r.color_g),
                        denormalize((*pixels)[row1 + 2], r.color_b) });
                }
                if (entry.has_size) {
                    a->set_size(denormalize((*pixels)[row1 + 3], r.size));
                }
                a->set_radius(radius);
                a->set_query_radius(query_radius);
                break;
            }
            }
            ++result.patched;

        } else {
            // ----------------------------------------------------------------
            // Construct missing entity, resolve callable, wire.
            // ----------------------------------------------------------------
            switch (entry.kind) {
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
                if (entry.has_color) {
                    emitter->set_color({ denormalize((*pixels)[row1 + 0], r.color_r),
                        denormalize((*pixels)[row1 + 1], r.color_g),
                        denormalize((*pixels)[row1 + 2], r.color_b) });
                }
                if (entry.has_size) {
                    emitter->set_size(denormalize((*pixels)[row1 + 3], r.size));
                }
                auto wiring = fabric.wire(emitter);
                const uint32_t new_id = emitter->id();
                if (new_id != entry.id) {
                    result.warnings.push_back("Emitter schema_id=" + std::to_string(entry.id)
                        + " reconstructed as runtime_id=" + std::to_string(new_id));
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
                const uint32_t new_id = sensor->id();
                if (new_id != entry.id) {
                    result.warnings.push_back("Sensor schema_id=" + std::to_string(entry.id)
                        + " reconstructed as runtime_id=" + std::to_string(new_id));
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
                auto agent = std::make_shared<Agent>(query_radius,
                    entry.perception_fn_name, std::move(pfn),
                    entry.influence_fn_name, std::move(ifn));
                agent->set_position(position);
                agent->set_intensity(intensity);
                agent->set_radius(radius);
                agent->set_query_radius(query_radius);
                if (entry.has_color) {
                    agent->set_color({ denormalize((*pixels)[row1 + 0], r.color_r),
                        denormalize((*pixels)[row1 + 1], r.color_g),
                        denormalize((*pixels)[row1 + 2], r.color_b) });
                }
                if (entry.has_size) {
                    agent->set_size(denormalize((*pixels)[row1 + 3], r.size));
                }
                auto wiring = fabric.wire(agent);
                const uint32_t new_id = agent->id();
                if (new_id != entry.id) {
                    result.warnings.push_back("Agent schema_id=" + std::to_string(entry.id)
                        + " reconstructed as runtime_id=" + std::to_string(new_id));
                }
                apply_wiring(std::move(wiring), entry.wiring, result.warnings);
                break;
            }
            }
            ++result.constructed;
        }
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder::reconstruct: constructed={} patched={} skipped={} warnings={}",
        result.constructed, result.patched, result.skipped, result.warnings.size());

    return result;
}

} // namespace MayaFlux::Nexus
