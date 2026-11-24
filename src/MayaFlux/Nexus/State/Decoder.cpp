#include "Decoder.hpp"

#include "MayaFlux/Nexus/Principals/Locus.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Transitive/Facet/ControlValueText.hpp"
#include "MayaFlux/Transitive/IO/JSONSerializer.hpp"

namespace MayaFlux::Nexus {

namespace {

    // -------------------------------------------------------------------------
    // Versions
    // -------------------------------------------------------------------------

    bool version_supported(uint32_t version)
    {
        return version >= State::k_schema_min_version && version <= State::k_schema_version;
    }

    std::string version_error(uint32_t version)
    {
        return "Unsupported schema version: " + std::to_string(version)
            + " (supported " + std::to_string(State::k_schema_min_version)
            + " to " + std::to_string(State::k_schema_version) + ")";
    }

    // -------------------------------------------------------------------------
    // Callables and wiring
    // -------------------------------------------------------------------------

    template <typename Fn>
    Fn resolve_fn(
        const std::shared_ptr<Fn>& resolved,
        const std::string& name,
        std::string_view label,
        std::vector<std::string>& warnings)
    {
        if (name.empty()) {
            return {};
        }
        if (!resolved || !*resolved) {
            warnings.push_back(std::string(label) + ": unknown '" + name + "', left empty");
            return {};
        }
        return *resolved;
    }

    void apply_wiring(
        Wiring wiring,
        const State::WiringRecord& rec,
        std::string_view who,
        std::vector<std::string>& warnings)
    {
        const auto fallback = [&](const std::string& why) {
            warnings.push_back(std::string(who) + " wiring: " + why + ", commit_driven instead");
        };

        switch (rec.kind) {
        case State::WiringKind::Every:
            if (!rec.interval) {
                fallback("every has no interval");
                break;
            }
            wiring.every(*rec.interval,
                rec.interval_token.value_or(Vruta::ProcessingToken::SAMPLE_ACCURATE));
            if (rec.duration) {
                wiring.for_duration(*rec.duration,
                    rec.duration_token.value_or(Vruta::ProcessingToken::SAMPLE_ACCURATE));
            }
            if (rec.times && *rec.times > 1) {
                wiring.times(*rec.times);
            }
            if (!rec.position_fn_name.empty()) {
                warnings.push_back(std::string(who) + " wiring: position function '"
                    + rec.position_fn_name + "' cannot be restored");
            }
            break;

        case State::WiringKind::MoveTo:
            if (rec.steps) {
                for (const auto& s : *rec.steps) {
                    wiring.move_to(s.position, s.delay_seconds);
                }
            }
            if (rec.times && *rec.times > 1) {
                wiring.times(*rec.times);
            }
            break;

        case State::WiringKind::Scroll:
            fallback("scroll needs a live window");
            break;
        case State::WiringKind::Key:
            fallback("a key trigger needs a live window");
            break;
        case State::WiringKind::Mouse:
            fallback("a mouse trigger needs a live window");
            break;
        case State::WiringKind::Network:
            fallback("a network trigger needs a live source");
            break;
        case State::WiringKind::WindowEvent:
            fallback("a window event trigger needs a live source");
            break;
        case State::WiringKind::Factory:
            fallback("factory '" + rec.factory_name + "' cannot be restored");
            break;
        case State::WiringKind::Bind:
            fallback("bind callables cannot be restored");
            break;
        case State::WiringKind::Unsupported:
            fallback("the saved wiring kind is unsupported");
            break;
        case State::WiringKind::CommitDriven:
            break;
        }
        wiring.finalise();
    }

    // -------------------------------------------------------------------------
    // Applying saved state to live entities
    // -------------------------------------------------------------------------

    std::string who_is(const State::EntityRecord& entry)
    {
        return entry.kind + " " + std::to_string(entry.id);
    }

    /**
     * @brief Give every control of the entity the value the record saved for it.
     *
     * A control the record does not list was empty when it was saved and is emptied.
     * A listed control the entity does not hold is reported and skipped.
     */
    void apply_controls(
        const QuantityStore& store,
        const State::EntityRecord& entry,
        std::vector<std::string>& warnings)
    {
        store.for_each([&](std::string_view name, Quantity& slot) {
            const auto saved = entry.controls.find(std::string(name));
            if (saved == entry.controls.end()) {
                slot.assign(Quantity::unset());
                return;
            }

            Quantity decoded;
            Reflect::TextError reason = Reflect::TextError::Invalid;
            if (Reflect::TextCodec<Quantity>::decode(saved->second, decoded, reason)) {
                slot.assign(decoded);
            } else {
                warnings.push_back(who_is(entry) + ": control '" + std::string(name)
                    + "' has unreadable text '" + saved->second + "', left as it is");
            }
        });

        for (const auto& [name, text] : entry.controls) {
            if (!store.find(name)) {
                warnings.push_back(who_is(entry) + ": control '" + name + "' is not held by the entity, skipped");
            }
        }
    }

    void apply_locus_nav(Locus& locus, const State::LocusNavRecord& nav)
    {
        auto& state = locus.nav();
        state.eye = nav.eye;
        state.fov_radians = nav.fov;
        state.near_plane = nav.near_plane;
        state.far_plane = nav.far_plane;
        state.move_speed = nav.speed;
        state.mouse_sensitivity = nav.mouse_sensitivity;
        state.scroll_speed = nav.scroll_speed;

        const glm::vec3 look = nav.target - nav.eye;
        if (glm::dot(look, look) > 0.0F) {
            const glm::vec3 dir = glm::normalize(look);
            state.yaw = std::atan2(dir.x, dir.z);
            state.pitch = std::asin(glm::clamp(dir.y, -1.0F, 1.0F));
        }
    }

    void apply_agent(
        Agent& agent,
        const State::EntityRecord& entry,
        std::vector<std::string>& warnings)
    {
        apply_controls(agent.store(), entry, warnings);

        if (auto* presence = dynamic_cast<Presence*>(&agent)) {
            const auto held = presence->get<PresenceControls>();
            if (const auto curve = quantity_value<Presence::FalloffCurve>(held.falloff_curve)) {
                presence->set_falloff_curve(*curve);
            }
        }

        if (entry.locus_nav) {
            if (auto* locus = dynamic_cast<Locus*>(&agent)) {
                apply_locus_nav(*locus, *entry.locus_nav);
            } else {
                warnings.push_back("Agent " + std::to_string(entry.id)
                    + " has locus_nav in schema but is not a Locus at runtime");
            }
        }
    }

    // -------------------------------------------------------------------------
    // Entities: patch an existing one, or construct a missing one
    // -------------------------------------------------------------------------

    void note_name_mismatch(
        const State::EntityRecord& entry,
        std::string_view what,
        const std::string& saved,
        const std::string& live,
        std::vector<std::string>& warnings)
    {
        if (!saved.empty() && saved != live) {
            warnings.push_back(who_is(entry) + " " + std::string(what)
                + " mismatch: schema='" + saved + "' live='" + live + "'");
        }
    }

    bool patch_entity(
        Fabric& fabric,
        const State::EntityRecord& entry,
        std::vector<std::string>& warnings)
    {
        switch (State::parse_kind(entry.kind)) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(entry.id);
            if (!e) {
                return false;
            }
            note_name_mismatch(entry, "fn_name", entry.influence_fn_name, e->fn_name(), warnings);
            apply_controls(e->store(), entry, warnings);
            return true;
        }
        case Fabric::Kind::Sensor: {
            auto s = fabric.get_sensor(entry.id);
            if (!s) {
                return false;
            }
            note_name_mismatch(entry, "fn_name", entry.perception_fn_name, s->fn_name(), warnings);
            apply_controls(s->store(), entry, warnings);
            return true;
        }
        case Fabric::Kind::Agent: {
            auto a = fabric.get_agent(entry.id);
            if (!a) {
                return false;
            }
            note_name_mismatch(entry, "perception_fn_name", entry.perception_fn_name, a->perception_fn_name(), warnings);
            note_name_mismatch(entry, "influence_fn_name", entry.influence_fn_name, a->influence_fn_name(), warnings);
            apply_agent(*a, entry, warnings);
            return true;
        }
        }
        return false;
    }

    void note_runtime_id(
        const State::EntityRecord& entry,
        uint32_t runtime_id,
        std::vector<std::string>& warnings)
    {
        if (runtime_id != entry.id) {
            warnings.push_back(who_is(entry) + " reconstructed as runtime_id=" + std::to_string(runtime_id));
        }
    }

    /** @brief The query radius an entity is built with. The record's controls give it its own right after. */
    constexpr float k_built_radius = 1.0F;

    std::shared_ptr<Agent> construct_agent(
        Fabric& fabric,
        const State::EntityRecord& entry,
        std::vector<std::string>& warnings)
    {
        const std::string who = who_is(entry);

        auto pfn = resolve_fn(fabric.resolve_perception_fn(entry.perception_fn_name),
            entry.perception_fn_name, who + " perception_fn", warnings);
        auto ifn = resolve_fn(fabric.resolve_influence_fn(entry.influence_fn_name),
            entry.influence_fn_name, who + " influence_fn", warnings);

        if (entry.subkind == "locus" && entry.locus_nav) {
            const auto& nav = *entry.locus_nav;
            const Kinesis::NavigationConfig config {
                .initial_eye = nav.eye,
                .initial_target = nav.target,
                .fov_radians = nav.fov,
                .near_plane = nav.near_plane,
                .far_plane = nav.far_plane,
                .move_speed = nav.speed,
                .mouse_sensitivity = nav.mouse_sensitivity,
                .scroll_speed = nav.scroll_speed,
            };
            warnings.push_back(who + ": view_targets must be reconnected by caller");
            return std::make_shared<Locus>(config, k_built_radius,
                entry.perception_fn_name, std::move(pfn),
                entry.influence_fn_name, std::move(ifn));
        }

        if (entry.subkind == "presence") {
            auto rfn = resolve_fn(fabric.resolve_radiate_fn(entry.radiate_fn_name),
                entry.radiate_fn_name, who + " radiate_fn", warnings);
            return std::make_shared<Presence>(k_built_radius,
                entry.perception_fn_name, std::move(pfn),
                entry.influence_fn_name, std::move(ifn),
                entry.radiate_fn_name, std::move(rfn));
        }

        if (entry.subkind == "locus") {
            warnings.push_back(who + ": no locus_nav in schema, reconstructed as plain Agent");
        } else if (!entry.subkind.empty()) {
            warnings.push_back(who + ": unknown subkind '" + entry.subkind
                + "', reconstructed as plain Agent");
        }

        if (entry.perception_fn_name.empty() && entry.influence_fn_name.empty()) {
            return std::make_shared<Agent>(k_built_radius);
        }
        return std::make_shared<Agent>(k_built_radius,
            entry.perception_fn_name, std::move(pfn),
            entry.influence_fn_name, std::move(ifn));
    }

    void construct_entity(
        Fabric& fabric,
        const State::EntityRecord& entry,
        std::vector<std::string>& warnings)
    {
        const std::string who = who_is(entry);

        if (!entry.audio_sinks.empty() || !entry.render_sinks.empty()) {
            warnings.push_back(who + ": sinks are not restored, they need a BufferManager and a window");
        }

        switch (State::parse_kind(entry.kind)) {
        case Fabric::Kind::Emitter: {
            std::shared_ptr<Emitter> emitter;
            if (entry.influence_fn_name.empty()) {
                emitter = std::make_shared<Emitter>();
            } else {
                emitter = std::make_shared<Emitter>(entry.influence_fn_name,
                    resolve_fn(fabric.resolve_influence_fn(entry.influence_fn_name),
                        entry.influence_fn_name, who + " influence_fn", warnings));
            }
            apply_controls(emitter->store(), entry, warnings);
            auto wiring = fabric.wire(emitter);
            note_runtime_id(entry, emitter->id(), warnings);
            apply_wiring(std::move(wiring), entry.wiring, who, warnings);
            break;
        }
        case Fabric::Kind::Sensor: {
            std::shared_ptr<Sensor> sensor;
            if (entry.perception_fn_name.empty()) {
                sensor = std::make_shared<Sensor>(k_built_radius);
            } else {
                sensor = std::make_shared<Sensor>(k_built_radius, entry.perception_fn_name,
                    resolve_fn(fabric.resolve_perception_fn(entry.perception_fn_name),
                        entry.perception_fn_name, who + " perception_fn", warnings));
            }
            apply_controls(sensor->store(), entry, warnings);
            auto wiring = fabric.wire(sensor);
            note_runtime_id(entry, sensor->id(), warnings);
            apply_wiring(std::move(wiring), entry.wiring, who, warnings);
            break;
        }
        case Fabric::Kind::Agent: {
            auto agent = construct_agent(fabric, entry, warnings);
            apply_agent(*agent, entry, warnings);
            auto wiring = fabric.wire(agent);
            note_runtime_id(entry, agent->id(), warnings);
            apply_wiring(std::move(wiring), entry.wiring, who, warnings);
            break;
        }
        }
    }

    // -------------------------------------------------------------------------
    // Expanses
    // -------------------------------------------------------------------------

    enum class Restored : uint8_t {
        Constructed,
        Patched,
        Skipped,
    };

    struct ExpanseParts {
        Expanse::ContainsFn contains;
        Expanse::CrossingFn on_enter;
        Expanse::CrossingFn on_exit;
    };

    template <typename Record>
    void apply_bounds(Expanse& expanse, const Record& rec)
    {
        if (rec.bounds_min && rec.bounds_max) {
            expanse.set({ .bounds = Kinesis::AABB3D { .min = *rec.bounds_min, .max = *rec.bounds_max } });
        }
    }

    /**
     * @brief Whether a record can become an Expanse at all.
     *
     * A Hull wraps a live buffer and cannot be rebuilt from data. A Mantle is
     * restored as its plain box. Anything else needs a predicate name or a box.
     */
    template <typename Record>
    bool expanse_restorable(const Record& rec, const std::string& label, std::vector<std::string>& warnings)
    {
        if (rec.subkind == "hull") {
            warnings.push_back(label + ": a Hull wraps a live buffer that is not encoded, skipping");
            return false;
        }
        if (rec.subkind == "mantle") {
            warnings.push_back(label + ": a Mantle is restored as a plain boxed Expanse, its look must be made again");
        } else if (!rec.subkind.empty()) {
            warnings.push_back(label + ": unknown subkind '" + rec.subkind + "', restored as a plain Expanse");
        }
        if (rec.fn_name.empty() && !(rec.bounds_min && rec.bounds_max)) {
            warnings.push_back(label + ": neither fn_name nor bounds, skipping");
            return false;
        }
        return true;
    }

    template <typename Record>
    bool resolve_expanse_parts(
        const Record& rec,
        const std::vector<Fabric*>& fabrics,
        const std::string& label,
        ExpanseParts& parts,
        std::vector<std::string>& warnings)
    {
        if (!rec.fn_name.empty()) {
            for (const Fabric* fabric : fabrics) {
                if (auto ptr = fabric->resolve_expanse_fn(rec.fn_name); ptr && *ptr) {
                    parts.contains = *ptr;
                    break;
                }
            }
            if (!parts.contains) {
                warnings.push_back(label + ": fn '" + rec.fn_name + "' not in registry, skipping");
                return false;
            }
        }

        const auto resolve_crossing = [&](const std::string& name, Expanse::CrossingFn& out) {
            if (name.empty()) {
                return;
            }
            for (const Fabric* fabric : fabrics) {
                if (auto ptr = fabric->resolve_crossing_fn(name); ptr && *ptr) {
                    out = *ptr;
                    return;
                }
            }
            warnings.push_back(label + ": crossing '" + name + "' not in registry, left empty");
        };
        resolve_crossing(rec.on_enter_fn_name, parts.on_enter);
        resolve_crossing(rec.on_exit_fn_name, parts.on_exit);
        return true;
    }

    /**
     * @brief A plain Expanse already on the Fabric that this record describes.
     *
     * Matched by predicate and crossing names. A box-only Expanse has no name to
     * match on, so its box must be equal.
     */
    std::shared_ptr<Expanse> find_equivalent(const Fabric& fabric, const State::ExpanseRecord& rec)
    {
        for (uint32_t id : fabric.all_expanse_ids()) {
            auto x = fabric.get_expanse(id);
            if (!x) {
                continue;
            }
            const Expanse& live = *x;
            if (typeid(live) != typeid(Expanse)) {
                continue;
            }
            if (x->fn_name() != rec.fn_name
                || x->on_enter_fn_name() != rec.on_enter_fn_name
                || x->on_exit_fn_name() != rec.on_exit_fn_name) {
                continue;
            }
            if (const auto box = x->bounds();
                rec.fn_name.empty() && (!box || box->min != *rec.bounds_min || box->max != *rec.bounds_max)) {
                continue;
            }
            return x;
        }
        return nullptr;
    }

    Restored restore_expanse(
        Fabric& fabric,
        const State::ExpanseRecord& rec,
        std::vector<std::string>& warnings)
    {
        const std::string label = "Expanse " + std::to_string(rec.id);
        if (!expanse_restorable(rec, label, warnings)) {
            return Restored::Skipped;
        }

        if (auto existing = find_equivalent(fabric, rec)) {
            apply_bounds(*existing, rec);
            return Restored::Patched;
        }

        ExpanseParts parts;
        if (!resolve_expanse_parts(rec, { &fabric }, label, parts, warnings)) {
            return Restored::Skipped;
        }

        auto expanse = std::make_shared<Expanse>(
            rec.fn_name,
            rec.on_enter_fn_name,
            rec.on_exit_fn_name,
            std::move(parts.contains),
            std::move(parts.on_enter),
            std::move(parts.on_exit));
        apply_bounds(*expanse, rec);
        fabric.add_expanse(std::move(expanse));
        return Restored::Constructed;
    }

    std::optional<float> read_cell_size(const std::string& json_path)
    {
        IO::JSONSerializer ser;
        auto schema = ser.read<State::FabricSchema>(json_path);
        return schema ? schema->cell_size : std::nullopt;
    }

    void log_warnings(const std::vector<std::string>& warnings)
    {
        for (const auto& w : warnings) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime, "StateDecoder: {}", w);
        }
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

    IO::JSONSerializer ser;
    auto schema_opt = ser.read<State::FabricSchema>(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }
    const auto& schema = *schema_opt;

    if (!version_supported(schema.version)) {
        m_last_error = version_error(schema.version);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    std::vector<std::string> warnings;

    for (const auto& entry : schema.entities) {
        if (!State::kind_known(entry.kind)) {
            warnings.push_back("unknown kind '" + entry.kind + "' for id "
                + std::to_string(entry.id) + ", skipping");
            ++m_missing_count;
            continue;
        }

        if (patch_entity(fabric, entry, warnings)) {
            ++m_patched_count;
        } else {
            warnings.push_back("id " + std::to_string(entry.id) + " not found as "
                + entry.kind + ", skipping");
            ++m_missing_count;
        }
    }

    for (const auto& xrec : schema.expanses) {
        restore_expanse(fabric, xrec, warnings);
    }

    log_warnings(warnings);

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder: patched {} entities ({} missing) from {}",
        m_patched_count, m_missing_count, json_path);

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

    IO::JSONSerializer ser;
    auto schema_opt = ser.read<State::FabricSchema>(json_path);
    if (!schema_opt) {
        m_last_error = "Failed to load schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }
    const auto& schema = *schema_opt;

    if (!version_supported(schema.version)) {
        m_last_error = version_error(schema.version);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return result;
    }

    const auto existing_ids = fabric.all_ids();
    const std::unordered_set<uint32_t> existing(existing_ids.begin(), existing_ids.end());

    for (const auto& entry : schema.entities) {
        if (!State::kind_known(entry.kind)) {
            result.warnings.push_back("Unknown kind '" + entry.kind
                + "' for id " + std::to_string(entry.id) + ", skipping");
            ++result.skipped;
            continue;
        }

        if (existing.contains(entry.id)) {
            if (patch_entity(fabric, entry, result.warnings)) {
                ++result.patched;
            } else {
                ++result.skipped;
            }
        } else {
            construct_entity(fabric, entry, result.warnings);
            ++result.constructed;
        }
    }

    for (const auto& xrec : schema.expanses) {
        switch (restore_expanse(fabric, xrec, result.warnings)) {
        case Restored::Constructed:
            ++result.constructed;
            break;
        case Restored::Patched:
            ++result.patched;
            break;
        case Restored::Skipped:
            ++result.skipped;
            break;
        }
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
    m_last_error.clear();

    const std::string tapestry_path = base_dir + "/tapestry.json";
    IO::JSONSerializer ser;
    auto schema_opt = ser.read<State::TapestrySchema>(tapestry_path);
    if (!schema_opt) {
        m_last_error = "Failed to load tapestry schema: " + ser.last_error();
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return total;
    }
    const auto& schema = *schema_opt;

    if (!version_supported(schema.version)) {
        m_last_error = version_error(schema.version);
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return total;
    }

    for (const auto& ref : schema.fabrics) {
        auto fabric = tapestry.get_fabric(ref.name);
        if (!fabric) {
            fabric = tapestry.create_fabric(ref.name,
                read_cell_size(ref.base_path + ".json").value_or(1.0F));
        }
        auto result = reconstruct(*fabric, ref.base_path);
        total.constructed += result.constructed;
        total.patched += result.patched;
        total.skipped += result.skipped;
        for (auto& w : result.warnings) {
            total.warnings.push_back(ref.name + ": " + std::move(w));
        }
    }

    for (const auto& xrec : schema.expanses) {
        const std::string label = "TapestryExpanse '" + xrec.name + "'";
        if (!expanse_restorable(xrec, label, total.warnings)) {
            ++total.skipped;
            continue;
        }

        std::vector<Fabric*> fabrics;
        for (const auto& fname : xrec.fabric_names) {
            if (auto fabric = tapestry.get_fabric(fname)) {
                fabrics.push_back(fabric.get());
            } else {
                total.warnings.push_back(label + ": fabric '" + fname + "' not found");
            }
        }

        std::shared_ptr<Expanse> expanse = tapestry.get_expanse(xrec.name);
        const bool existed = expanse != nullptr;

        if (existed) {
            apply_bounds(*expanse, xrec);
        } else {
            ExpanseParts parts;
            if (!resolve_expanse_parts(xrec, fabrics, label, parts, total.warnings)) {
                ++total.skipped;
                continue;
            }

            if (xrec.fn_name.empty()) {
                expanse = tapestry.create_expanse<Expanse>(
                    xrec.name,
                    Kinesis::AABB3D { .min = *xrec.bounds_min, .max = *xrec.bounds_max },
                    std::move(parts.on_enter),
                    std::move(parts.on_exit));
            } else {
                expanse = tapestry.create_expanse(
                    xrec.name,
                    std::move(parts.contains),
                    std::move(parts.on_enter),
                    std::move(parts.on_exit));
                apply_bounds(*expanse, xrec);
            }

            if (!expanse) {
                ++total.skipped;
                continue;
            }
            expanse->set_fn_name(xrec.fn_name);
            expanse->set_on_enter_fn_name(xrec.on_enter_fn_name);
            expanse->set_on_exit_fn_name(xrec.on_exit_fn_name);
        }

        for (Fabric* fabric : fabrics) {
            fabric->add_expanse(expanse);
        }
        if (existed) {
            ++total.patched;
        } else {
            ++total.constructed;
        }
    }

    total.user_state = schema.user_state;

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder::reconstruct(Tapestry): constructed={} patched={} skipped={} warnings={}",
        total.constructed, total.patched, total.skipped, total.warnings.size());
    return total;
}

} // namespace MayaFlux::Nexus
