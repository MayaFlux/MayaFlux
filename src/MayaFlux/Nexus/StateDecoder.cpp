#include "StateDecoder.hpp"

#include "MayaFlux/IO/ImageReader.hpp"
#include "MayaFlux/IO/JSONSerializer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

namespace {

    constexpr uint32_t k_exr_rows = 3;
    constexpr uint32_t k_channels = 4;

    // -------------------------------------------------------------------------
    // Schema structs  (mirrored from StateEncoder.cpp until centralized)
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
                IO::member("wiring", &EntityRecord::wiring));
        }
    };

    struct FabricSchema {
        uint32_t version { 0 };
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
    // Helpers
    // -------------------------------------------------------------------------

    float denormalize(float norm, const Range& r)
    {
        return r.min + norm * (r.max - r.min);
    }

    Fabric::Kind parse_kind(const std::string& s)
    {
        if (s == "sensor") {
            return Fabric::Kind::Sensor;
        }
        if (s == "agent") {
            return Fabric::Kind::Agent;
        }
        return Fabric::Kind::Emitter;
    }

    bool kind_known(const std::string& s)
    {
        return s == "emitter" || s == "sensor" || s == "agent";
    }

    void apply_wiring(Wiring wiring, const WiringRecord& rec, std::vector<std::string>& warnings)
    {
        if (rec.kind == "every") {
            wiring.every(*rec.interval);
            if (rec.duration) {
                wiring.for_duration(*rec.duration);
            }
            if (rec.times && *rec.times > 1) {
                wiring.times(*rec.times);
            }
        } else if (rec.kind == "move_to") {
            if (rec.steps) {
                for (const auto& s : *rec.steps) {
                    wiring.move_to(s.position, s.delay_seconds);
                }
            }
            if (rec.times && *rec.times > 1) {
                wiring.times(*rec.times);
            }
        } else if (rec.kind != "commit_driven") {
            warnings.emplace_back("Unsupported wiring kind '" + rec.kind + "'; falling back to commit_driven");
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
        if (image_opt->channels != k_channels) {
            error_out = "EXR channel count mismatch: expected "
                + std::to_string(k_channels) + " got " + std::to_string(image_opt->channels);
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
        return PixelView { std::move(*image_opt), nullptr, w };
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
    auto schema_opt = ser.read<FabricSchema>(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    const auto& schema = *schema_opt;

    if (schema.version < 2 || schema.version > 4) {
        m_last_error = "Unsupported schema version: " + std::to_string(schema.version);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (schema.entities.empty()) {
        m_last_error = "Schema contains no entities: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const uint32_t expected_rows = schema.version >= 4 ? 5 : k_exr_rows;
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

        if (!kind_known(entry.kind)) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
                "StateDecoder: unknown kind '{}' for id {}, skipping", entry.kind, entry.id);
            ++m_missing_count;
            continue;
        }

        const size_t row0 = (static_cast<size_t>(0) * width + i) * k_channels;
        const size_t row1 = (static_cast<size_t>(1) * width + i) * k_channels;
        const size_t row2 = (static_cast<size_t>(2) * width + i) * k_channels;

        const glm::vec3 position {
            denormalize((*pixels)[row0 + 0], r.pos_x),
            denormalize((*pixels)[row0 + 1], r.pos_y),
            denormalize((*pixels)[row0 + 2], r.pos_z),
        };

        switch (parse_kind(entry.kind)) {
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
    auto schema_opt = ser.read<FabricSchema>(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    const auto& schema = *schema_opt;

    if (schema.version < 2 || schema.version > 4) {
        m_last_error = "Unsupported schema version: " + std::to_string(schema.version);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }

    if (schema.entities.empty()) {
        m_last_error = "Schema contains no entities: " + json_path;
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }

    const uint32_t expected_rows = schema.version >= 4 ? 5 : k_exr_rows;
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

        if (!kind_known(entry.kind)) {
            result.warnings.push_back("Unknown kind '" + entry.kind
                + "' for id " + std::to_string(entry.id) + ", skipping");
            ++result.skipped;
            continue;
        }

        const size_t row0 = (static_cast<size_t>(0) * width + i) * k_channels;
        const size_t row1 = (static_cast<size_t>(1) * width + i) * k_channels;
        const size_t row2 = (static_cast<size_t>(2) * width + i) * k_channels;

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
            switch (parse_kind(entry.kind)) {
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
            switch (parse_kind(entry.kind)) {
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
                auto agent = std::make_shared<Agent>(query_radius,
                    entry.perception_fn_name, std::move(pfn),
                    entry.influence_fn_name, std::move(ifn));
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

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder::reconstruct: constructed={} patched={} skipped={} warnings={}",
        result.constructed, result.patched, result.skipped, result.warnings.size());

    return result;
}

} // namespace MayaFlux::Nexus
