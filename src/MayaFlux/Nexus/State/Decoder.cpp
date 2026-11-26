#include "Decoder.hpp"

#include "AssemblySource.hpp"
#include "Encoder.hpp"
#include "Orchestrator.hpp"

#include "MayaFlux/Nexus/Expanses/Hull.hpp"
#include "MayaFlux/Nexus/Expanses/Mantle.hpp"
#include "MayaFlux/Nexus/Principals/Locus.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

#include "MayaFlux/IO/Detail/IOTransitions.hpp"
#include "MayaFlux/IO/FileReader.hpp"
#include "MayaFlux/IO/Image/ImageReader.hpp"
#include "MayaFlux/IO/Spatial/SpatialReader.hpp"
#include "MayaFlux/IO/Spatial/SpatialSequence.hpp"
#include "MayaFlux/IO/Volume/VolumeReader.hpp"
#include "MayaFlux/IO/Model/ModelReader.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Morphology.hpp"
#include "MayaFlux/Kriya/Course.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"

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
    // Calls and wiring
    // -------------------------------------------------------------------------

    struct Loading {
        const State::Bindings* bindings {};
        std::shared_ptr<Buffers::BufferManager> buffer_manager;
        std::vector<std::string>& warnings;
        State::BufferNames buffers; ///< Bound buffers and those loaded from assets.
        const State::CourseNames* courses {}; ///< Made for the saved courses nothing binds.
        std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<Buffers::RenderProcessor>>* sinks {}; ///< Restored, by saved entity id and place.
    };

    std::shared_ptr<Kriya::Course> course_named(
        const State::Bindings* bindings, const State::CourseNames* made, const std::string& name)
    {
        if (bindings) {
            if (const auto found = bindings->courses.find(name); found != bindings->courses.end() && found->second) {
                return found->second;
            }
        }
        if (made) {
            if (const auto found = made->find(name); found != made->end()) {
                return found->second;
            }
        }
        return nullptr;
    }

    State::BufferNames buffers_with(const State::Bindings* bindings, const State::BufferNames& loaded)
    {
        State::BufferNames buffers = loaded;
        if (bindings) {
            for (const auto& [name, buffer] : bindings->buffers) {
                buffers.insert_or_assign(name, buffer);
            }
        }
        return buffers;
    }

    Reflect::FunctionCall call_of(const State::CallRecord& record)
    {
        return { record.name, { record.arguments.begin(), record.arguments.end() } };
    }

    bool bound_function(const Loading& load, const State::CallRecord& record)
    {
        return load.bindings && load.bindings->functions.contains(record.name);
    }

    template <typename Slot>
    std::optional<Slot> make_bound(const Loading& load, const State::CallRecord& record, std::string& error)
    {
        return load.bindings->functions.make<Slot>(call_of(record), error,
            load.buffers, load.bindings->windows, load.bindings->sources);
    }

    /** The function a call names, made from the bound functions, or nullopt with a warning. */
    template <typename Slot>
    std::optional<Slot> made(const Loading& load, const State::CallRecord& record, const std::string& label)
    {
        if (record.name.empty()) {
            return std::nullopt;
        }
        if (!bound_function(load, record)) {
            load.warnings.push_back(label + ": unknown '" + record.name + "', left empty");
            return std::nullopt;
        }
        std::string error;
        auto function = make_bound<Slot>(load, record, error);
        if (!function) {
            load.warnings.push_back(label + ": " + error);
        }
        return function;
    }

    /** made(), falling back to a Fabric registry entry for a function without arguments. */
    template <typename Slot>
    Slot resolve_fn(const Loading& load, const State::CallRecord& record,
        const std::shared_ptr<Slot>& registered, const std::string& label)
    {
        if (!bound_function(load, record) && record.arguments.empty() && registered && *registered) {
            return *registered;
        }
        return made<Slot>(load, record, label).value_or(Slot {});
    }

    template <typename Map>
    typename Map::mapped_type bound_object(const Map* names, const std::string& name)
    {
        if (!names) {
            return {};
        }
        const auto found = names->find(name);
        return found == names->end() ? typename Map::mapped_type {} : found->second;
    }

    std::shared_ptr<Buffers::VKBuffer> bound_buffer(const Loading& load, const std::string& name)
    {
        return bound_object(&load.buffers, name);
    }

    std::shared_ptr<Core::Window> bound_window(const Loading& load, const std::string& name)
    {
        return bound_object(load.bindings ? &load.bindings->windows : nullptr, name);
    }

    /** Run the saved configure() calls on an object. */
    template <typename Host>
    void apply_configurations(Host& host, const std::vector<State::CallRecord>& calls,
        const Loading& load, const std::string& label)
    {
        const auto had = host.configurations();
        for (const auto& call : calls) {
            if (std::ranges::find(had, call_of(call)) != had.end()) {
                continue;
            }
            if (auto apply = made<std::function<void(Host&)>>(load, call, label + " configure")) {
                host.configure(call_of(call), *apply);
            }
        }
    }

    /** Add back the registered structs a record holds values of. */
    void extend_controls(const State::Extendable& target,
        const std::map<std::string, std::string>& controls, const Loading& load)
    {
        if (!load.bindings) {
            return;
        }
        for (const auto& [name, text] : controls) {
            const size_t dot = name.find('.');
            if (dot == std::string::npos) {
                continue;
            }
            const auto found = load.bindings->controls.find(std::string_view(name).substr(0, dot));
            if (found != load.bindings->controls.end()) {
                found->second(target);
            }
        }
    }

    bool apply_trigger(Wiring& wiring, const State::TriggerRecord& rec, const Loading& load, const std::string& label)
    {
        if (rec.kind == State::TriggerKind::Network) {
            const auto source = bound_object(load.bindings ? &load.bindings->sources : nullptr, rec.source);
            if (!source) {
                load.warnings.push_back(label + ": network source '" + rec.source + "' is not bound");
                return false;
            }
            wiring.on(source);
            return true;
        }

        if (rec.kind == State::TriggerKind::Course) {
            const auto course = course_named(load.bindings, load.courses, rec.source);
            if (!course || !rec.course_event) {
                load.warnings.push_back(label + ": course '" + rec.source + "' is not there");
                return false;
            }
            wiring.on(course->subscribe(*rec.course_event));
            return true;
        }

        const auto window = bound_window(load, rec.source);
        if (!window) {
            load.warnings.push_back(label + ": window '" + rec.source + "' is not bound");
            return false;
        }

        switch (rec.kind) {
        case State::TriggerKind::Key: {
            if (!rec.key) {
                return false;
            }
            const IO::ModifiedKey key = rec.modifiers
                ? IO::ModifiedKey { *rec.key, *rec.modifiers }
                : IO::ModifiedKey { *rec.key };
            if (auto release = made<std::function<void()>>(load, rec.callback, label + " key release")) {
                wiring.on(window, key, rec.held, call_of(rec.callback), std::move(*release));
            } else if (rec.held) {
                wiring.on(window, key, true);
            } else {
                wiring.on(window, key);
            }
            return true;
        }
        case State::TriggerKind::Mouse: {
            if (!rec.button) {
                return false;
            }
            if (auto release = made<std::function<void(double, double)>>(load, rec.callback, label + " mouse release")) {
                wiring.on(window, *rec.button, rec.held, call_of(rec.callback), std::move(*release));
            } else if (rec.held) {
                wiring.on(window, *rec.button, true);
            } else {
                wiring.on(window, *rec.button);
            }
            return true;
        }
        case State::TriggerKind::Scroll: {
            auto fn = made<std::function<void(double, double)>>(load, rec.callback, label + " scroll");
            if (!fn) {
                return false;
            }
            wiring.on_scroll(window, call_of(rec.callback), std::move(*fn));
            return true;
        }
        case State::TriggerKind::WindowEvent: {
            Vruta::WindowEventFilter filter;
            filter.event_type = rec.event_type;
            filter.key_code = rec.key;
            filter.button = rec.button;
            filter.modifiers = rec.modifiers;
            wiring.on(window, std::move(filter));
            return true;
        }
        case State::TriggerKind::Network:
        case State::TriggerKind::Course:
            break;
        }
        return false;
    }

    template <typename Factory>
    bool apply_factory(Wiring& wiring, const State::CallRecord& rec, const Loading& load, const std::string& label)
    {
        auto factory = made<Factory>(load, rec, label + " factory");
        if (!factory) {
            return false;
        }
        wiring.use(call_of(rec), std::move(*factory));
        return true;
    }

    bool apply_placement_source(Wiring& wiring, const State::CallRecord& rec, const Loading& load, const std::string& label)
    {
        if (rec.name.empty()) {
            return false;
        }
        if (bound_function(load, rec)) {
            std::string error;
            if (auto place = make_bound<Wiring::PlacementFn>(load, rec, error)) {
                wiring.place_from(call_of(rec), std::move(*place));
                return true;
            }
            if (auto position = make_bound<Wiring::PositionFn>(load, rec, error)) {
                wiring.position_from(call_of(rec), std::move(*position));
                return true;
            }
            load.warnings.push_back(label + " placement source: " + error);
            return false;
        }
        load.warnings.push_back(label + " placement source: unknown '" + rec.name + "'");
        return false;
    }

    void apply_wiring(
        Wiring wiring,
        const State::WiringRecord& rec,
        const std::string& who,
        const Loading& load)
    {
        const std::string label = who + " wiring";
        for (const auto& what : rec.lost) {
            load.warnings.push_back(label + ": " + what + " was not saved");
        }

        const bool was_scheduled = rec.interval || !rec.steps.empty() || !rec.triggers.empty()
            || rec.factory_kind != State::FactoryKind::None || rec.bound;
        bool scheduled = false;

        switch (rec.factory_kind) {
        case State::FactoryKind::Sound:
            scheduled |= apply_factory<Wiring::SoundFactory>(wiring, rec.factory, load, label);
            break;
        case State::FactoryKind::Graphics:
            scheduled |= apply_factory<Wiring::GraphicsFactory>(wiring, rec.factory, load, label);
            break;
        case State::FactoryKind::Cross:
            scheduled |= apply_factory<Wiring::CrossFactory>(wiring, rec.factory, load, label);
            break;
        case State::FactoryKind::Event:
            scheduled |= apply_factory<Wiring::EventFactory>(wiring, rec.factory, load, label);
            break;
        case State::FactoryKind::None:
            break;
        }

        apply_placement_source(wiring, rec.place, load, label);

        if (rec.bound) {
            if (rec.bound_to_entity) {
                wiring.bind();
                scheduled = true;
            } else if (auto attach = made<std::function<void()>>(load, rec.bind_attach, label + " bind")) {
                if (auto detach = made<std::function<void()>>(load, rec.bind_detach, label + " bind detach")) {
                    wiring.bind(call_of(rec.bind_attach), std::move(*attach),
                        call_of(rec.bind_detach), std::move(*detach));
                } else {
                    wiring.bind(call_of(rec.bind_attach), std::move(*attach));
                }
                scheduled = true;
            }
        }

        for (const auto& call : rec.configurations) {
            if (auto apply = made<std::function<void(Wiring&)>>(load, call, label + " configure")) {
                wiring.configure(call_of(call), *apply);
                scheduled = true;
            }
        }

        for (const auto& trigger : rec.triggers) {
            scheduled |= apply_trigger(wiring, trigger, load, label);
        }

        if (rec.interval) {
            wiring.every(*rec.interval,
                rec.interval_token.value_or(Vruta::ProcessingToken::SAMPLE_ACCURATE));
            scheduled = true;
        }
        for (const auto& step : rec.steps) {
            wiring.move_to(step.position, step.delay_seconds);
            scheduled = true;
        }
        if (!rec.steps.empty() && !rec.interval && rec.interval_token) {
            wiring.every(0.0, *rec.interval_token);
        }
        if (rec.duration) {
            wiring.for_duration(*rec.duration,
                rec.duration_token.value_or(Vruta::ProcessingToken::SAMPLE_ACCURATE));
        }
        if (rec.times) {
            wiring.times(*rec.times);
        }

        if (auto fn = made<std::function<void()>>(load, rec.on_complete, label + " on_complete")) {
            wiring.on_complete(call_of(rec.on_complete), std::move(*fn));
        }
        if (auto fn = made<std::function<void()>>(load, rec.on_detach, label + " on_detach")) {
            wiring.on_detach(call_of(rec.on_detach), std::move(*fn));
        }

        if ((was_scheduled || !rec.lost.empty()) && !scheduled) {
            load.warnings.push_back(label + ": nothing left to schedule it, commit_driven instead");
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
            note_name_mismatch(entry, "fn_name", entry.influence.name, e->fn_name(), warnings);
            apply_controls(e->store(), entry, warnings);
            return true;
        }
        case Fabric::Kind::Sensor: {
            auto s = fabric.get_sensor(entry.id);
            if (!s) {
                return false;
            }
            note_name_mismatch(entry, "fn_name", entry.perception.name, s->fn_name(), warnings);
            apply_controls(s->store(), entry, warnings);
            return true;
        }
        case Fabric::Kind::Agent: {
            auto a = fabric.get_agent(entry.id);
            if (!a) {
                return false;
            }
            note_name_mismatch(entry, "perception_fn_name", entry.perception.name, a->perception_fn_name(), warnings);
            note_name_mismatch(entry, "influence_fn_name", entry.influence.name, a->influence_fn_name(), warnings);
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
        const Loading& load)
    {
        const std::string who = who_is(entry);

        auto pfn = resolve_fn(load, entry.perception,
            fabric.resolve_perception_fn(entry.perception.name), who + " perception_fn");
        auto ifn = resolve_fn(load, entry.influence,
            fabric.resolve_influence_fn(entry.influence.name), who + " influence_fn");

        std::shared_ptr<Agent> agent;
        const auto registered = load.bindings
            ? load.bindings->agents.find(entry.subkind)
            : decltype(load.bindings->agents.end()) {};
        if (load.bindings && registered != load.bindings->agents.end()) {
            agent = registered->second.make();
            if (!agent) {
                load.warnings.push_back(who + ": '" + entry.subkind + "' made nothing, reconstructed as plain Agent");
                return std::make_shared<Agent>(k_built_radius);
            }
            return agent;
        }
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
            agent = std::make_shared<Locus>(config, k_built_radius, std::move(pfn), std::move(ifn));
        } else if (entry.subkind == "presence") {
            auto rfn = resolve_fn(load, entry.radiate,
                fabric.resolve_radiate_fn(entry.radiate.name), who + " radiate_fn");
            auto presence = std::make_shared<Presence>(k_built_radius,
                entry.perception.name, std::move(pfn),
                entry.influence.name, std::move(ifn),
                entry.radiate.name, std::move(rfn));
            presence->set_radiate_fn_call(call_of(entry.radiate));
            agent = std::move(presence);
        } else {
            if (entry.subkind == "locus") {
                load.warnings.push_back(who + ": no locus_nav in schema, reconstructed as plain Agent");
            } else if (!entry.subkind.empty()) {
                load.warnings.push_back(who + ": unknown subkind '" + entry.subkind
                    + "', reconstructed as plain Agent");
            }
            if (!pfn && !ifn && entry.perception.name.empty() && entry.influence.name.empty()) {
                agent = std::make_shared<Agent>(k_built_radius);
            } else {
                agent = std::make_shared<Agent>(k_built_radius,
                    entry.perception.name, std::move(pfn),
                    entry.influence.name, std::move(ifn));
            }
        }
        agent->set_perception_fn_call(call_of(entry.perception));
        agent->set_influence_fn_call(call_of(entry.influence));
        return agent;
    }

    template <typename Entity>
    void restore_attachment(Entity& entity, const State::EntityRecord& entry, const Loading& load)
    {
        if (!entry.attach) {
            return;
        }
        const auto buffer = bound_buffer(load, entry.attach->buffer);
        if (!buffer) {
            load.warnings.push_back(who_is(entry) + ": attached buffer '" + entry.attach->buffer + "' is not bound");
            return;
        }
        entity.attach(buffer, { .index = entry.attach->index, .slot = entry.attach->slot, .offset = entry.attach->offset, .on_move = entry.attach->on_move });
    }

    template <typename Entity>
    void restore_sinks(Entity& entity, const State::EntityRecord& entry, const Loading& load)
    {
        if (entry.audio_sinks.empty() && entry.render_sinks.empty()) {
            return;
        }
        const std::string who = who_is(entry);
        if (!load.buffer_manager) {
            load.warnings.push_back(who + ": sinks need the Fabric's BufferManager");
            return;
        }

        for (const auto& sink : entry.audio_sinks) {
            const State::CallRecord call { .name = sink.fn_name, .arguments = sink.arguments };
            using Producer = std::function<Kakshya::DataVariant(const InfluenceContext&)>;
            if (auto fn = made<Producer>(load, call, who + " audio sink")) {
                entity.sink_audio(*load.buffer_manager, sink.channel, call_of(call), std::move(*fn));
            } else {
                entity.sink_audio(*load.buffer_manager, sink.channel);
            }
        }

        uint32_t place = 0;
        for (const auto& sink : entry.render_sinks) {
            const uint32_t saved = place++;
            Portal::Graphics::RenderConfig config;
            config.target_window = bound_window(load, sink.window);
            for (const auto& name : sink.targets) {
                if (auto target = bound_object(load.bindings ? &load.bindings->targets : nullptr, name)) {
                    config.target_images.push_back(std::move(target));
                } else {
                    load.warnings.push_back(who + ": image target '" + name + "' is not bound");
                }
            }
            if (!sink.window.empty() && !config.target_window) {
                load.warnings.push_back(who + ": render sink window '" + sink.window + "' is not bound");
            }
            if (!config.target_window && config.target_images.empty()) {
                load.warnings.push_back(who + ": render sink has nothing to draw into, skipped");
                continue;
            }
            config.vertex_shader = sink.vertex_shader;
            config.fragment_shader = sink.fragment_shader;
            config.geometry_shader = sink.geometry_shader;
            config.default_texture_binding = sink.default_texture_binding;
            config.topology = sink.topology;
            config.polygon_mode = sink.polygon_mode;
            config.cull_mode = sink.cull_mode;
            config.triangulate = sink.triangulate;
            config.draw_priority = sink.draw_priority;
            config.extra_string_params = sink.extra_string_params;

            const State::CallRecord call { .name = sink.fn_name, .arguments = sink.arguments };
            if (auto fn = made<RenderFn>(load, call, who + " render sink")) {
                entity.render(*load.buffer_manager, config, call_of(call), std::move(*fn));
            } else {
                entity.render(*load.buffer_manager, config);
            }
            if (load.sinks) {
                load.sinks->insert_or_assign({ entry.id, saved }, entity.render_sinks().back().renderer);
            }
        }
    }

    /** Point the restored Locus of a record at the entity sinks it viewed. */
    void restore_view_sinks(Fabric& fabric, const State::EntityRecord& entry, uint32_t runtime_id, const Loading& load)
    {
        if (entry.view_sinks.empty()) {
            return;
        }
        const auto locus = std::dynamic_pointer_cast<Locus>(fabric.get_agent(runtime_id));
        for (const auto& view : entry.view_sinks) {
            const auto found = load.sinks ? load.sinks->find({ view.entity, view.sink }) : decltype(load.sinks->end()) {};
            if (locus && load.sinks && found != load.sinks->end()) {
                if (std::ranges::find(locus->view_targets(), found->second) == locus->view_targets().end()) {
                    locus->add_view_target(found->second);
                }
            } else {
                load.warnings.push_back(who_is(entry) + ": the sink it viewed on entity "
                    + std::to_string(view.entity) + " was not restored");
            }
        }
    }

    void restore_view_targets(Agent& agent, const State::EntityRecord& entry, const Loading& load)
    {
        auto* locus = dynamic_cast<Locus*>(&agent);
        if (!locus) {
            return;
        }
        for (const auto& name : entry.view_targets) {
            const auto buffer = bound_buffer(load, name);
            if (const auto processor = buffer ? buffer->get_render_processor() : nullptr) {
                locus->add_view_target(processor);
            } else {
                load.warnings.push_back(who_is(entry) + ": view target '" + name
                    + "' is not a bound buffer with a render processor");
            }
        }
    }

    std::shared_ptr<Agent> construct_entity(
        Fabric& fabric,
        const State::EntityRecord& entry,
        const Loading& load,
        uint32_t& runtime_id)
    {
        const std::string who = who_is(entry);

        switch (State::parse_kind(entry.kind)) {
        case Fabric::Kind::Emitter: {
            std::shared_ptr<Emitter> emitter;
            if (entry.influence.name.empty()) {
                emitter = std::make_shared<Emitter>();
            } else {
                emitter = std::make_shared<Emitter>(entry.influence.name,
                    resolve_fn(load, entry.influence,
                        fabric.resolve_influence_fn(entry.influence.name), who + " influence_fn"));
                emitter->set_fn_call(call_of(entry.influence));
            }
            extend_controls(emitter.get(), entry.controls, load);
            apply_controls(emitter->store(), entry, load.warnings);
            restore_attachment(*emitter, entry, load);
            apply_configurations(*emitter, entry.configurations, load, who);
            auto wiring = fabric.wire(emitter);
            runtime_id = emitter->id();
            note_runtime_id(entry, emitter->id(), load.warnings);
            apply_wiring(std::move(wiring), entry.wiring, who, load);
            restore_sinks(*emitter, entry, load);
            break;
        }
        case Fabric::Kind::Sensor: {
            std::shared_ptr<Sensor> sensor;
            if (entry.perception.name.empty()) {
                sensor = std::make_shared<Sensor>(k_built_radius);
            } else {
                sensor = std::make_shared<Sensor>(k_built_radius, entry.perception.name,
                    resolve_fn(load, entry.perception,
                        fabric.resolve_perception_fn(entry.perception.name), who + " perception_fn"));
                sensor->set_fn_call(call_of(entry.perception));
            }
            extend_controls(sensor.get(), entry.controls, load);
            apply_controls(sensor->store(), entry, load.warnings);
            apply_configurations(*sensor, entry.configurations, load, who);
            auto wiring = fabric.wire(sensor);
            runtime_id = sensor->id();
            note_runtime_id(entry, sensor->id(), load.warnings);
            apply_wiring(std::move(wiring), entry.wiring, who, load);
            break;
        }
        case Fabric::Kind::Agent: {
            auto agent = construct_agent(fabric, entry, load);
            extend_controls(agent.get(), entry.controls, load);
            apply_agent(*agent, entry, load.warnings);
            restore_attachment(*agent, entry, load);
            apply_configurations(*agent, entry.configurations, load, who);
            restore_view_targets(*agent, entry, load);
            auto wiring = fabric.wire(agent);
            runtime_id = agent->id();
            note_runtime_id(entry, agent->id(), load.warnings);
            apply_wiring(std::move(wiring), entry.wiring, who, load);
            restore_sinks(*agent, entry, load);
            return agent;
        }
        }
        return nullptr;
    }

    /** What patch_entity() leaves but the sinks: attachment, setups and views of a live entity. */
    void patch_references(Fabric& fabric, const State::EntityRecord& entry, const Loading& load)
    {
        const std::string who = who_is(entry);
        const auto placed = [&](const auto& entity) {
            if (entry.attach) {
                restore_attachment(*entity, entry, load);
            } else {
                entity->detach();
            }
            apply_configurations(*entity, entry.configurations, load, who);
        };

        switch (State::parse_kind(entry.kind)) {
        case Fabric::Kind::Emitter:
            if (const auto emitter = fabric.get_emitter(entry.id)) {
                placed(emitter);
            }
            break;
        case Fabric::Kind::Sensor:
            if (const auto sensor = fabric.get_sensor(entry.id)) {
                apply_configurations(*sensor, entry.configurations, load, who);
            }
            break;
        case Fabric::Kind::Agent:
            if (const auto agent = fabric.get_agent(entry.id)) {
                placed(agent);
                if (const auto locus = std::dynamic_pointer_cast<Locus>(agent)) {
                    for (const auto& name : entry.view_targets) {
                        const auto buffer = bound_buffer(load, name);
                        const auto processor = buffer ? buffer->get_render_processor() : nullptr;
                        if (!processor) {
                            load.warnings.push_back(who + ": view target '" + name
                                + "' is not a bound buffer with a render processor");
                        } else if (std::ranges::find(locus->view_targets(), processor) == locus->view_targets().end()) {
                            locus->add_view_target(processor);
                        }
                    }
                }
            }
            break;
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
        std::shared_ptr<Buffers::VKBuffer> buffer;
    };

    template <typename Record>
    void apply_bounds(Expanse& expanse, const Record& rec)
    {
        if (rec.bounds_min && rec.bounds_max) {
            expanse.set({ .bounds = Kinesis::AABB3D { .min = *rec.bounds_min, .max = *rec.bounds_max } });
        }
    }

    /** What a record can be rebuilt as with what is bound: its own kind, or a plain Expanse. */
    template <typename Record>
    std::string rebuilt_subkind(const Record& rec, const Loading& load)
    {
        if (load.bindings && load.bindings->expanses.contains(rec.subkind)) {
            return rec.subkind;
        }
        return rec.subkind.empty() || !bound_buffer(load, rec.buffer) ? std::string {} : rec.subkind;
    }

    /**
     * @brief Whether a record can become an Expanse at all.
     *
     * A Hull needs its buffer bound. A Mantle without one is restored as its plain
     * box. Anything else needs a predicate name or a box.
     */
    template <typename Record>
    bool expanse_restorable(const Record& rec, const std::string& label, const Loading& load)
    {
        if (load.bindings && load.bindings->expanses.contains(rec.subkind)) {
            return true;
        }
        const bool bound = bound_buffer(load, rec.buffer) != nullptr;
        if (rec.subkind == "hull") {
            if (!bound) {
                load.warnings.push_back(label + ": the buffer of a Hull is not bound, skipping");
            }
            return bound;
        }
        if (rec.subkind == "mantle") {
            if (!bound) {
                load.warnings.push_back(label + ": the buffer of a Mantle is not bound, restored as a plain boxed Expanse");
            }
        } else if (!rec.subkind.empty()) {
            load.warnings.push_back(label + ": unknown subkind '" + rec.subkind + "', restored as a plain Expanse");
        }
        if (rec.contains.name.empty() && !(rec.bounds_min && rec.bounds_max)) {
            load.warnings.push_back(label + ": neither fn_name nor bounds, skipping");
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
        const Loading& load)
    {
        const auto registered = [&fabrics](auto resolve, const std::string& name) {
            decltype(resolve(std::declval<const Fabric&>(), name)) found;
            for (const Fabric* fabric : fabrics) {
                if (found = resolve(*fabric, name); found && *found) {
                    break;
                }
            }
            return found;
        };
        const auto contains_in = [](const Fabric& fabric, const std::string& name) { return fabric.resolve_expanse_fn(name); };
        const auto crossing_in = [](const Fabric& fabric, const std::string& name) { return fabric.resolve_crossing_fn(name); };

        parts.buffer = bound_buffer(load, rec.buffer);
        if (!rec.contains.name.empty() && rec.subkind != "hull") {
            parts.contains = resolve_fn(load, rec.contains,
                registered(contains_in, rec.contains.name), label + " contains");
            if (!parts.contains) {
                load.warnings.push_back(label + ": no predicate, skipping");
                return false;
            }
        }
        if (!rec.on_enter.name.empty()) {
            parts.on_enter = resolve_fn(load, rec.on_enter,
                registered(crossing_in, rec.on_enter.name), label + " on_enter");
        }
        if (!rec.on_exit.name.empty()) {
            parts.on_exit = resolve_fn(load, rec.on_exit,
                registered(crossing_in, rec.on_exit.name), label + " on_exit");
        }
        return true;
    }

    template <typename Record>
    Portal::Graphics::FitMode fit_of(const Record& rec)
    {
        return Reflect::string_to_enum_case_insensitive<Portal::Graphics::FitMode>(rec.fit_name)
            .value_or(Portal::Graphics::FitMode::STRETCH);
    }

    template <typename Record>
    Kinesis::AABB3D box_of(const Record& rec)
    {
        return Kinesis::AABB3D {
            .min = rec.bounds_min.value_or(glm::vec3(0.0F)),
            .max = rec.bounds_max.value_or(glm::vec3(0.0F)),
        };
    }

    /** An Expanse of a registered class, or nullptr. */
    std::shared_ptr<Expanse> make_registered(const std::string& subkind, const Loading& load)
    {
        if (!load.bindings) {
            return nullptr;
        }
        const auto found = load.bindings->expanses.find(subkind);
        return found == load.bindings->expanses.end() ? nullptr : found->second.make();
    }

    template <typename Record>
    void keep_calls(Expanse& expanse, const Record& rec)
    {
        expanse.set_fn_call(call_of(rec.contains));
        expanse.set_on_enter_fn_call(call_of(rec.on_enter));
        expanse.set_on_exit_fn_call(call_of(rec.on_exit));
    }

    template <typename Record>
    void finish_expanse(Expanse& expanse, const Record& rec, const Loading& load, const std::string& label)
    {
        extend_controls(&expanse, rec.controls, load);
        for (const auto& [name, text] : rec.controls) {
            if (Quantity* slot = expanse.store().find(name)) {
                Quantity decoded;
                Reflect::TextError reason = Reflect::TextError::Invalid;
                if (Reflect::TextCodec<Quantity>::decode(text, decoded, reason)) {
                    slot->assign(decoded);
                }
            }
        }
        apply_configurations(expanse, rec.configurations, load, label);
    }

    /**
     * @brief An Expanse already on the Fabric that this record describes.
     *
     * Matched by kind and calls. A box-only Expanse has no name to match on, so
     * its box must be equal. A Hull or Mantle must also wrap the same buffer,
     * which @p wraps answers.
     */
    template <typename Wraps>
    std::shared_ptr<Expanse> find_equivalent(const Fabric& fabric, const State::ExpanseRecord& rec,
        const std::string& subkind, const Wraps& wraps)
    {
        for (uint32_t id : fabric.all_expanse_ids()) {
            auto x = fabric.get_expanse(id);
            if (!x || !wraps(*x, subkind)) {
                continue;
            }
            if (x->fn_call() != call_of(rec.contains)
                || x->on_enter_fn_call() != call_of(rec.on_enter)
                || x->on_exit_fn_call() != call_of(rec.on_exit)) {
                continue;
            }
            if (const auto box = x->bounds(); subkind != "hull" && rec.contains.name.empty()
                && (!box || box->min != *rec.bounds_min || box->max != *rec.bounds_max)) {
                continue;
            }
            return x;
        }
        return nullptr;
    }

    template <typename Wraps>
    Restored restore_expanse(
        Fabric& fabric,
        const State::ExpanseRecord& rec,
        const Loading& load,
        const Wraps& wraps)
    {
        const std::string label = "Expanse " + std::to_string(rec.id);
        if (!expanse_restorable(rec, label, load)) {
            return Restored::Skipped;
        }

        const std::string subkind = rebuilt_subkind(rec, load);
        if (auto existing = find_equivalent(fabric, rec, subkind, wraps)) {
            if (subkind != "hull") {
                apply_bounds(*existing, rec);
            }
            finish_expanse(*existing, rec, load, label);
            return Restored::Patched;
        }

        ExpanseParts parts;
        if (!resolve_expanse_parts(rec, { &fabric }, label, parts, load)) {
            return Restored::Skipped;
        }

        std::shared_ptr<Expanse> expanse = make_registered(subkind, load);
        if (expanse) {
            apply_bounds(*expanse, rec);
        } else if (subkind == "hull") {
            expanse = std::make_shared<Hull>(parts.buffer, std::move(parts.on_enter),
                std::move(parts.on_exit), rec.collection);
        } else if (subkind == "mantle") {
            expanse = std::make_shared<Mantle>(box_of(rec), parts.buffer, fit_of(rec),
                std::move(parts.on_enter), std::move(parts.on_exit));
        } else {
            expanse = std::make_shared<Expanse>(rec.contains.name, rec.on_enter.name, rec.on_exit.name,
                std::move(parts.contains), std::move(parts.on_enter), std::move(parts.on_exit));
            apply_bounds(*expanse, rec);
        }
        keep_calls(*expanse, rec);
        finish_expanse(*expanse, rec, load, label);
        fabric.add_expanse(std::move(expanse));
        return Restored::Constructed;
    }

    // -------------------------------------------------------------------------
    // Assets
    // -------------------------------------------------------------------------

    /** A buffer for each stream of a snapshot archive, the later ones named name.stream. */
    bool load_spatial(const State::AssetRecord& record, const std::string& path, State::BufferNames& loaded)
    {
        std::vector<std::string> streams;
        {
            IO::SpatialReader reader;
            if (!reader.open(path)) {
                return false;
            }
            streams = reader.get_stream_names();
            reader.close();
        }

        bool any = false;
        for (const auto& stream : streams) {
            const auto frames = IO::SpatialFileFrames::create(path, stream);
            const auto frame = frames && frames->count() > 0 ? frames->read(0) : nullptr;
            if (!frame || frame->positions.empty()) {
                continue;
            }
            const auto box = Kinesis::aabb(frame->positions);
            auto buffer = std::make_shared<Buffers::RecordedGeometryBuffer>(
                static_cast<uint32_t>(frame->positions.size()), frame->topology,
                Kinesis::SamplerBounds { .min = box.min, .max = box.max });
            buffer->write_frame(frame);
            loaded.emplace(any ? record.name + "." + stream : record.name, std::move(buffer));
            any = true;
        }
        return any;
    }

    glm::mat4 matrix_at(const std::vector<float>& values, size_t first)
    {
        glm::mat4 matrix { 1.0F };
        for (glm::length_t column = 0; column < 4; ++column) {
            for (glm::length_t row = 0; row < 4; ++row) {
                matrix[column][row] = values[first + static_cast<size_t>(column * 4 + row)];
            }
        }
        return matrix;
    }

    std::shared_ptr<Buffers::VKBuffer> load_instances(const State::AssetRecord& record, const std::string& path)
    {
        IO::ModelReader reader;
        if (!reader.can_read(path) || !reader.open(path)) {
            return nullptr;
        }
        const auto assembly = reader.create_mesh_assembly();
        reader.close();
        if (!assembly) {
            return nullptr;
        }

        std::optional<size_t> source;
        for (size_t row = 0; row < assembly->get_total_elements() && !source; ++row) {
            const auto placement = assembly->placement(row);
            if (placement && placement->source && assembly->mesh(*placement->source)) {
                source = *placement->source;
            }
        }
        if (!source) {
            return nullptr;
        }

        auto node = std::make_shared<Nodes::GpuSync::MeshWriterNode>();
        node->set_mesh(*assembly->mesh(*source));
        node->compute_frame();

        auto network = std::make_shared<Nodes::Network::InstanceNetwork>();
        for (const auto& instance : record.instances) {
            const uint32_t index = network->add_slot(instance.name, node);
            auto& slot = network->get_slot(index);
            if (instance.transform.size() == 16) {
                slot.transform = matrix_at(instance.transform, 0);
            }
            for (size_t first = 0; first + 16 <= instance.palette.size(); first += 16) {
                slot.palette.push_back(matrix_at(instance.palette, first));
            }
            slot.tint = instance.tint;
            slot.scalar = instance.scalar;
            slot.texture = instance.texture;
            slot.morph_weights = instance.morph_weights;
        }
        if (network->slot_count() > 0) {
            auto& first = network->get_slot(0);
            first.skin = assembly->skin(*source);
            if (const auto* morphs
                = assembly->get_data().sources[*source].facets.get<Kakshya::MeshMorphTargets>()) {
                first.morphs = std::make_shared<const Kakshya::MeshMorphTargets>(*morphs);
            }
        }
        return std::make_shared<Buffers::InstanceNetworkBuffer>(network);
    }

    std::shared_ptr<Buffers::VKBuffer> load_texture(const std::string& path)
    {
        const auto image = IO::ImageReader::load(path, 0);
        if (!image) {
            return nullptr;
        }
        return std::make_shared<Buffers::TextureBuffer>(image->width, image->height, image->format, image->data());
    }

    /** A volume has to be registered before it can be filled, so this one is, and is not drawn. */
    std::shared_ptr<Buffers::VKBuffer> load_volume_asset(const State::AssetRecord& record,
        const std::string& path, const std::shared_ptr<Buffers::BufferManager>& manager)
    {
        IO::VolumeReader reader;
        if (!manager || !reader.can_read(path) || record.resolution.size() != 3
            || !record.bounds_min || !record.bounds_max) {
            return nullptr;
        }
        const Kinesis::Lattice3D lattice {
            .resolution = { record.resolution[0], record.resolution[1], record.resolution[2] },
            .bounds = { .min = *record.bounds_min, .max = *record.bounds_max },
        };
        const auto data = reader.load(path, lattice);
        return data ? IO::Detail::create_volume_grid_buffer(manager, *data) : nullptr;
    }

    /** One realization for each file and way of realizing it. */
    std::string realization_key(const std::string& path, const State::CallRecord& call)
    {
        std::string key = path + '\n' + call.name;
        for (const auto& [name, value] : call.arguments) {
            key += '\n' + name + '=' + value;
        }
        return key;
    }

    /** The assembly of a file realized with the options a record names, and its materials bound. */
    std::shared_ptr<Kriya::MeshAssemblyRealization> realize_file(const std::string& path,
        const State::CallRecord& call, const std::optional<glm::vec3>& low, const std::optional<glm::vec3>& high,
        const Loading& load)
    {
        const auto assembly = Detail::read_assembly(path);
        if (!assembly) {
            return nullptr;
        }

        Kriya::MeshAssemblyRealizeOptions options;
        if (auto saved = made<Kriya::MeshAssemblyRealizeOptions>(load, call, "realize options of " + path)) {
            options = std::move(*saved);
        }
        if (low && high) {
            options.bounds = Kinesis::SamplerBounds { .min = *low, .max = *high };
        }

        auto realization = Kriya::MeshAssemblyRealization::create(assembly, options);
        if (realization) {
            realization->bind_materials(IO::Detail::make_material_resolver(path));
        }
        return realization;
    }

    /** The buffers of the records whose names are not bound, made from their files. */
    State::BufferNames load_assets(
        const std::vector<State::AssetRecord>& records,
        const std::string& schema_path,
        const State::Bindings* bindings,
        const std::shared_ptr<Buffers::BufferManager>& manager,
        std::map<std::string, std::shared_ptr<Kriya::MeshAssemblyRealization>>& realized,
        std::vector<std::string>& warnings)
    {
        State::BufferNames loaded;
        const auto directory = std::filesystem::path(IO::FileReader::resolve_path(schema_path)).parent_path();

        for (const auto& record : records) {
            if (record.form == State::AssetForm::Call || (bindings && bindings->buffers.contains(record.name))) {
                continue;
            }
            const std::string path = record.snapshot ? (directory / record.file).string() : record.file;
            std::shared_ptr<Buffers::VKBuffer> buffer;

            const Loading realizing {
                .bindings = bindings,
                .buffer_manager = nullptr,
                .warnings = warnings,
                .buffers = buffers_with(bindings, {}),
            };
            if (record.form == State::AssetForm::Spatial) {
                if (!load_spatial(record, path, loaded)) {
                    warnings.push_back("asset '" + record.name + "' could not be loaded from " + path);
                }
                continue;
            }
            if (record.form == State::AssetForm::Instances) {
                buffer = load_instances(record, path);
            } else if (record.form == State::AssetForm::Texture) {
                buffer = load_texture(path);
            } else if (record.form == State::AssetForm::Volume) {
                buffer = load_volume_asset(record, path, manager);
            } else if (record.form == State::AssetForm::Part) {
                auto& realization = realized[realization_key(path, record.realize)];
                if (!realization) {
                    realization = realize_file(
                        path, record.realize, record.bounds_min, record.bounds_max, realizing);
                }
                buffer = StateOrchestrator::buffer_of(realization, record.index).first;
            } else if (IO::ModelReader reader; reader.can_read(path) && reader.open(path)) {
                const auto resolver = IO::Detail::make_default_resolver(path);
                if (record.form == State::AssetForm::Network) {
                    if (const auto network = reader.create_mesh_network(resolver)) {
                        buffer = std::make_shared<Buffers::MeshNetworkBuffer>(network);
                    }
                } else if (const auto meshes = reader.create_mesh_buffers(resolver); record.index < meshes.size()) {
                    buffer = meshes[record.index];
                }
                reader.close();
            }

            if (buffer) {
                loaded.emplace(record.name, std::move(buffer));
            } else {
                warnings.push_back("asset '" + record.name + "' could not be loaded from " + path);
            }
        }
        const Loading load {
            .bindings = bindings,
            .buffer_manager = nullptr,
            .warnings = warnings,
            .buffers = buffers_with(bindings, loaded),
        };
        for (const auto& record : records) {
            if (record.form != State::AssetForm::Call || (bindings && bindings->buffers.contains(record.name))) {
                continue;
            }
            auto buffer = made<std::shared_ptr<Buffers::VKBuffer>>(load, record.make, "asset '" + record.name + "'");
            if (buffer && *buffer) {
                loaded.emplace(record.name, std::move(*buffer));
            }
        }
        return loaded;
    }

    // -------------------------------------------------------------------------
    // Courses
    // -------------------------------------------------------------------------

    /** Courses for the saved ones nothing binds, before anything that listens to them is restored. */
    void make_courses(
        const std::vector<State::CourseRecord>& records,
        const State::Bindings* bindings,
        State::CourseNames& made)
    {
        for (const auto& record : records) {
            if (course_named(bindings, &made, record.name) || !record.range_in || !record.range_out) {
                continue;
            }
            if (auto course = Kriya::Course::create(record.step_rate,
                    { .in = *record.range_in, .out = *record.range_out }, record.paused)) {
                made.insert_or_assign(record.name, std::move(course));
            }
        }
    }

    /** Restore every saved course onto the bound one of its name, or the one made for it. */
    void apply_courses(
        const std::vector<State::CourseRecord>& records,
        const State::Bindings* bindings,
        std::vector<std::string>& warnings,
        const State::CourseNames* made_courses)
    {
        const auto named = [bindings, made_courses](const std::string& name) {
            return course_named(bindings, made_courses, name);
        };

        for (const auto& record : records) {
            const std::string label = "course '" + record.name + "'";
            const auto course = named(record.name);
            if (!course) {
                warnings.push_back(label + " is not bound and could not be made, its playback is not restored");
                continue;
            }

            for (const auto& what : record.lost) {
                warnings.push_back(label + ": " + what + " was not saved");
            }
            const Loading load {
                .bindings = bindings,
                .buffer_manager = nullptr,
                .warnings = warnings,
                .buffers = buffers_with(bindings, {}),
            };
            extend_controls(course.get(), record.controls, load);
            apply_configurations(*course, record.configurations, load, label);

            for (const auto& [control, text] : record.controls) {
                ControlValue* slot = course->store().find(control);
                ControlValue decoded;
                Reflect::TextError reason = Reflect::TextError::Invalid;
                if (slot && Reflect::TextCodec<ControlValue>::decode(text, decoded, reason)) {
                    slot->assign(decoded);
                } else {
                    warnings.push_back(label + ": control '" + control + "' could not be applied");
                }
            }
            if (record.range_in && record.range_out && !course->range(*record.range_in, *record.range_out)) {
                warnings.push_back(label + ": its range was refused");
            }

            const auto held = course->markers();
            for (const auto& marker : record.markers) {
                const bool present = std::ranges::any_of(held, [&marker](const Kriya::CourseMarker& live) {
                    return live.name == marker.name && live.time == marker.time;
                });
                if (!present) {
                    course->add_marker(marker.name, marker.time);
                }
            }
        }

        for (const auto& record : records) {
            const auto course = named(record.name);
            if (!course) {
                continue;
            }
            const std::string label = "course '" + record.name + "'";
            if (!record.follows.empty()) {
                if (!course->follow(named(record.follows), record.follow_offset)) {
                    warnings.push_back(label + ": the course '" + record.follows + "' it follows is not there");
                }
            }

            bool queued = record.paused ? course->pause() : course->start();
            if (record.playback) {
                queued = queued && course->resume(*record.playback);
                if (record.playback->fade_seconds > 0.0) {
                    queued = queued && course->fade_weight(record.playback->fade_to, record.playback->fade_seconds);
                }
            } else if (record.follows.empty()) {
                queued = queued && course->seek(record.time);
            }
            if (!queued) {
                warnings.push_back(label + ": its playback could not be queued");
            }
        }
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

bool StateDecoder::wraps(const Expanse& live, const std::string& subkind,
    const std::shared_ptr<Buffers::VKBuffer>& buffer) const
{
    if (m_bindings) {
        if (const auto found = m_bindings->expanses.find(subkind); found != m_bindings->expanses.end()) {
            return std::type_index(typeid(live)) == found->second.type;
        }
    }
    if (subkind == "hull") {
        const auto* hull = dynamic_cast<const Hull*>(&live);
        return hull && hull->m_buf == buffer;
    }
    if (subkind == "mantle") {
        const auto* mantle = dynamic_cast<const Mantle*>(&live);
        return mantle && mantle->m_look.buf == buffer;
    }
    return typeid(live) == typeid(Expanse);
}

StateDecoder::Moving StateDecoder::start_motions(const std::vector<State::MotionRecord>& records,
    const std::string& schema_path, const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
    Realizations& realized, State::CourseNames& courses, ReconstructionResult& result) const
{
    Moving moving;
    const auto directory = std::filesystem::path(IO::FileReader::resolve_path(schema_path)).parent_path();
    const StateOrchestrator orchestrator { scheduler };

    for (const auto& record : records) {
        if (m_bindings && m_bindings->motions.contains(record.name)) {
            continue;
        }
        const std::string path = record.snapshot ? (directory / record.file).string() : record.file;
        auto& realization = realized[realization_key(path, record.realize)];
        if (!realization) {
            const Loading realizing {
                .bindings = m_bindings.get(),
                .buffer_manager = nullptr,
                .warnings = result.warnings,
                .buffers = buffers_with(m_bindings.get(), result.buffers),
            };
            realization = realize_file(path, record.realize, record.bounds_min, record.bounds_max, realizing);
        }

        const auto cell = std::make_shared<Memory::SnapshotCell<State::Carried>>();
        auto reflows = orchestrator.start_motion(realization, record.motion, record.edge, true, cell);
        const auto& [placing, deforming, streaming] = reflows;

        std::vector<std::shared_ptr<Kriya::Course>> made;
        const auto keep = [&made](const auto& reflow) {
            if (reflow) {
                for (auto& course : reflow->courses()) {
                    made.push_back(std::move(course));
                }
            }
        };
        keep(placing);
        std::ranges::for_each(deforming, keep);
        std::ranges::for_each(streaming, keep);

        if (made.empty()) {
            result.warnings.push_back("motion '" + record.name + "' could not be started again from " + path);
            continue;
        }
        for (size_t i = 0; i < std::min(made.size(), record.courses.size()); ++i) {
            courses.insert_or_assign(record.courses[i], made[i]);
        }
        for (size_t i = 0; i < std::min(record.buffers.size(), record.rows.size()); ++i) {
            if (m_bindings && m_bindings->buffers.contains(record.buffers[i])) {
                continue;
            }
            if (auto shown = StateOrchestrator::buffer_of(realization, record.rows[i]).first) {
                result.buffers.insert_or_assign(record.buffers[i], std::move(shown));
            }
        }
        result.motions.insert_or_assign(record.name, std::pair { realization, std::move(reflows) });
        moving.emplace(record.name, cell);
    }
    return moving;
}

void StateDecoder::carry(const std::vector<State::MotionRecord>& records, const Moving& moving,
    const std::map<std::pair<std::string, uint32_t>, uint32_t>& ids,
    const std::function<Fabric*(const std::string&)>& fabric_named)
{
    for (const auto& record : records) {
        const auto cell = moving.find(record.name);
        if (cell == moving.end()) {
            continue;
        }
        State::Carried carried;
        for (const auto& held : record.carried) {
            const auto id = ids.find({ held.fabric, held.entity });
            Fabric* fabric = id == ids.end() ? nullptr : fabric_named(held.fabric);
            if (!fabric) {
                continue;
            }
            if (const auto emitter = fabric->get_emitter(id->second)) {
                carried.emplace_back(held.row, std::weak_ptr<Emitter>(emitter));
            } else if (const auto sensor = fabric->get_sensor(id->second)) {
                carried.emplace_back(held.row, std::weak_ptr<Sensor>(sensor));
            } else if (const auto agent = fabric->get_agent(id->second)) {
                carried.emplace_back(held.row, std::weak_ptr<Agent>(agent));
            }
        }
        cell->second->publish(std::move(carried));
    }
}

void StateDecoder::patch_sinks(Fabric& fabric, const State::EntityRecord& entry,
    std::vector<std::string>& warnings, const State::BufferNames& buffers,
    std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<Buffers::RenderProcessor>>& sinks)
{
    const StateEncoder live(m_bindings);
    const Loading load {
        .bindings = m_bindings.get(),
        .buffer_manager = fabric.m_buffers,
        .warnings = warnings,
        .buffers = buffers,
        .sinks = &sinks,
    };

    const auto patch = [&](const auto& entity) {
        const auto held = live.sink_records(fabric, entry.id);
        if (held.audio_sinks != entry.audio_sinks || held.render_sinks != entry.render_sinks) {
            if (!load.buffer_manager) {
                warnings.push_back(who_is(entry) + ": sinks need the Fabric's BufferManager");
                return;
            }
            auto& manager = *load.buffer_manager;
            std::erase_if(entity->m_audio_sinks, [&](const AudioSink& sink) {
                if (entity->made_by_configure(sink.buf.get())) {
                    return false;
                }
                manager.remove_buffer(sink.buf, Buffers::ProcessingToken::AUDIO_BACKEND, sink.channel);
                return true;
            });
            std::erase_if(entity->m_render_sinks, [&](const RenderSink& sink) {
                if (entity->made_by_configure(sink.buf.get())) {
                    return false;
                }
                manager.remove_buffer(sink.buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);
                return true;
            });
            restore_sinks(*entity, entry, load);
            return;
        }

        uint32_t place = 0;
        for (const auto& sink : entity->render_sinks()) {
            if (!entity->made_by_configure(sink.buf.get())) {
                sinks.insert_or_assign({ entry.id, place++ }, sink.renderer);
            }
        }
    };

    if (const auto emitter = fabric.get_emitter(entry.id)) {
        patch(emitter);
    } else if (const auto agent = fabric.get_agent(entry.id)) {
        patch(agent);
    }
}

void StateDecoder::rewire(Fabric& fabric, const State::EntityRecord& entry,
    std::vector<std::string>& warnings, const State::BufferNames& buffers, const State::CourseNames& courses)
{
    const StateEncoder live(m_bindings);
    if (live.build_wiring(fabric, entry.id) == entry.wiring) {
        return;
    }
    const Loading load {
        .bindings = m_bindings.get(),
        .buffer_manager = fabric.m_buffers,
        .warnings = warnings,
        .buffers = buffers,
        .courses = &courses,
    };
    apply_wiring(fabric.rewire(entry.id), entry.wiring, who_is(entry), load);
}

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
    const Loading load {
        .bindings = m_bindings.get(),
        .buffer_manager = fabric.m_buffers,
        .warnings = warnings,
        .buffers = buffers_with(m_bindings.get(), {}),
    };

    for (const auto& entry : schema.entities) {
        if (!State::kind_known(entry.kind)) {
            warnings.push_back("unknown kind '" + entry.kind + "' for id "
                + std::to_string(entry.id) + ", skipping");
            ++m_missing_count;
            continue;
        }

        if (patch_entity(fabric, entry, warnings)) {
            std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<Buffers::RenderProcessor>> sinks;
            patch_references(fabric, entry, load);
            patch_sinks(fabric, entry, warnings, load.buffers, sinks);
            rewire(fabric, entry, warnings, load.buffers, {});
            ++m_patched_count;
        } else {
            warnings.push_back("id " + std::to_string(entry.id) + " not found as "
                + entry.kind + ", skipping");
            ++m_missing_count;
        }
    }

    for (const auto& xrec : schema.expanses) {
        restore_expanse(fabric, xrec, load,
            [this, buffer = bound_buffer(load, xrec.buffer)](const Expanse& live, const std::string& subkind) {
                return wraps(live, subkind, buffer);
            });
    }

    apply_courses(schema.courses, m_bindings.get(), warnings, nullptr);

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
    State::CourseNames courses;
    std::map<std::pair<std::string, uint32_t>, uint32_t> ids;
    return reconstruct_fabric(fabric, base_path, {}, courses, ids, {});
}

StateDecoder::ReconstructionResult StateDecoder::reconstruct_fabric(
    Fabric& fabric, const std::string& base_path, const State::BufferNames& loaded, State::CourseNames& courses,
    std::map<std::pair<std::string, uint32_t>, uint32_t>& ids, const std::string& label)
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

    Realizations realized;
    result.buffers = load_assets(schema.assets, json_path, m_bindings.get(), fabric.m_buffers, realized, result.warnings);
    State::BufferNames given = loaded;
    given.insert(result.buffers.begin(), result.buffers.end());
    Loading load {
        .bindings = m_bindings.get(),
        .buffer_manager = fabric.m_buffers,
        .warnings = result.warnings,
        .buffers = buffers_with(m_bindings.get(), given),
    };

    const auto moving = start_motions(schema.motions, json_path, fabric.m_scheduler, realized, courses, result);
    for (const auto& [name, buffer] : result.buffers) {
        if (!m_bindings || !m_bindings->buffers.contains(name)) {
            load.buffers.insert_or_assign(name, buffer);
        }
    }
    make_courses(schema.courses, m_bindings.get(), courses);
    std::map<std::pair<uint32_t, uint32_t>, std::shared_ptr<Buffers::RenderProcessor>> sinks;
    load.courses = &courses;
    load.sinks = &sinks;

    const auto existing_ids = fabric.all_ids();
    const std::unordered_set<uint32_t> existing(existing_ids.begin(), existing_ids.end());

    std::map<uint32_t, std::shared_ptr<Agent>> restored;
    std::vector<std::pair<const State::EntityRecord*, uint32_t>> constructed_ids;

    for (const auto& entry : schema.entities) {
        if (!State::kind_known(entry.kind)) {
            result.warnings.push_back("Unknown kind '" + entry.kind
                + "' for id " + std::to_string(entry.id) + ", skipping");
            ++result.skipped;
            continue;
        }

        if (existing.contains(entry.id)) {
            if (patch_entity(fabric, entry, result.warnings)) {
                patch_references(fabric, entry, load);
                patch_sinks(fabric, entry, result.warnings, load.buffers, sinks);
                rewire(fabric, entry, result.warnings, load.buffers, courses);
                constructed_ids.emplace_back(&entry, entry.id);
                ++result.patched;
            } else {
                ++result.skipped;
            }
        } else {
            uint32_t runtime_id = 0;
            if (auto agent = construct_entity(fabric, entry, load, runtime_id)) {
                restored.emplace(entry.id, std::move(agent));
            }
            constructed_ids.emplace_back(&entry, runtime_id);
            ++result.constructed;
        }
    }

    for (const auto& [entry, runtime_id] : constructed_ids) {
        restore_view_sinks(fabric, *entry, runtime_id, load);
    }

    for (const auto& entry : schema.entities) {
        if (!entry.view_of) {
            continue;
        }
        const auto shared = restored.find(*entry.view_of);
        const auto follower = restored.find(entry.id);
        const auto from = shared == restored.end() ? nullptr : std::dynamic_pointer_cast<Locus>(shared->second);
        const auto into = follower == restored.end() ? nullptr : std::dynamic_pointer_cast<Locus>(follower->second);
        if (from && into && from->view()) {
            into->set_view(from->view());
        } else {
            result.warnings.push_back(who_is(entry) + ": the view it shared could not be shared again");
        }
    }

    {
        std::map<uint32_t, uint32_t> runtime;
        for (const auto& [entry, id] : constructed_ids) {
            runtime.insert_or_assign(entry->id, id);
        }
        const auto locus_of = [&fabric, &runtime](uint32_t saved) {
            const auto found = runtime.find(saved);
            return found == runtime.end() ? nullptr
                                          : std::dynamic_pointer_cast<Locus>(fabric.get_agent(found->second));
        };
        for (const auto& entry : schema.entities) {
            if (!entry.follows_view) {
                continue;
            }
            const auto follower = locus_of(entry.id);
            const auto leader = locus_of(*entry.follows_view);
            const glm::mat4 offset = entry.follow_offset.size() == 16 ? matrix_at(entry.follow_offset, 0) : glm::mat4(1.0F);
            if (!follower || !leader || !follower->follow(leader->view(), offset)) {
                result.warnings.push_back(who_is(entry) + ": the view it followed could not be followed again");
            }
        }
    }

    for (const auto& xrec : schema.expanses) {
        const auto same = [this, buffer = bound_buffer(load, xrec.buffer)](const Expanse& live, const std::string& subkind) {
            return wraps(live, subkind, buffer);
        };
        switch (restore_expanse(fabric, xrec, load, same)) {
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

    for (const auto& [entry, runtime_id] : constructed_ids) {
        ids.insert_or_assign({ label, entry->id }, runtime_id);
    }
    carry(schema.motions, moving, ids, [&fabric](const std::string&) { return &fabric; });

    apply_courses(schema.courses, m_bindings.get(), result.warnings, &courses);
    result.courses = courses;

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

    Realizations realized;
    total.buffers = load_assets(schema.assets, tapestry_path, m_bindings.get(), tapestry.m_buffers, realized, total.warnings);

    State::CourseNames courses;
    std::map<std::pair<std::string, uint32_t>, uint32_t> ids;
    const auto moving = start_motions(
        schema.motions, tapestry_path, tapestry.m_scheduler, realized, courses, total);
    make_courses(schema.courses, m_bindings.get(), courses);

    for (const auto& ref : schema.fabrics) {
        auto fabric = tapestry.get_fabric(ref.name);
        if (!fabric) {
            fabric = tapestry.create_fabric(ref.name,
                read_cell_size(ref.base_path + ".json").value_or(1.0F));
        }
        auto result = reconstruct_fabric(*fabric, ref.base_path, total.buffers, courses, ids, ref.name);
        total.constructed += result.constructed;
        total.patched += result.patched;
        total.skipped += result.skipped;
        for (auto& w : result.warnings) {
            total.warnings.push_back(ref.name + ": " + std::move(w));
        }
    }

    const Loading load {
        .bindings = m_bindings.get(),
        .buffer_manager = nullptr,
        .warnings = total.warnings,
        .buffers = buffers_with(m_bindings.get(), total.buffers),
    };

    for (const auto& xrec : schema.expanses) {
        const std::string label = "TapestryExpanse '" + xrec.name + "'";
        if (!expanse_restorable(xrec, label, load)) {
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
            finish_expanse(*expanse, xrec, load, label);
        } else {
            ExpanseParts parts;
            if (!resolve_expanse_parts(xrec, fabrics, label, parts, load)) {
                ++total.skipped;
                continue;
            }

            const std::string subkind = rebuilt_subkind(xrec, load);
            if (auto registered = make_registered(subkind, load)) {
                apply_bounds(*registered, xrec);
                expanse = tapestry.adopt_expanse(xrec.name, std::move(registered));
            } else if (subkind == "hull") {
                expanse = tapestry.create_expanse<Hull>(xrec.name, parts.buffer,
                    std::move(parts.on_enter), std::move(parts.on_exit), xrec.collection);
            } else if (subkind == "mantle") {
                expanse = tapestry.create_expanse<Mantle>(xrec.name, box_of(xrec), parts.buffer, fit_of(xrec),
                    std::move(parts.on_enter), std::move(parts.on_exit));
            } else if (xrec.contains.name.empty()) {
                expanse = tapestry.create_expanse<Expanse>(xrec.name, box_of(xrec),
                    std::move(parts.on_enter), std::move(parts.on_exit));
            } else {
                expanse = tapestry.create_expanse(xrec.name, std::move(parts.contains),
                    std::move(parts.on_enter), std::move(parts.on_exit));
                apply_bounds(*expanse, xrec);
            }

            if (!expanse) {
                ++total.skipped;
                continue;
            }
            keep_calls(*expanse, xrec);
            finish_expanse(*expanse, xrec, load, label);
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

    carry(schema.motions, moving, ids, [&tapestry](const std::string& name) {
        return tapestry.get_fabric(name).get();
    });

    apply_courses(schema.courses, m_bindings.get(), total.warnings, &courses);
    total.courses = courses;
    total.user_state = schema.user_state;

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateDecoder::reconstruct(Tapestry): constructed={} patched={} skipped={} warnings={}",
        total.constructed, total.patched, total.skipped, total.warnings.size());
    return total;
}

} // namespace MayaFlux::Nexus
