#include "Encoder.hpp"

#include "MayaFlux/Nexus/Pheme/Sinks.hpp"

#include "MayaFlux/Nexus/Expanses/Hull.hpp"
#include "MayaFlux/Nexus/Expanses/Mantle.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

#include "MayaFlux/Nexus/Principals/Locus.hpp"
#include "MayaFlux/Nexus/Principals/Presence.hpp"

#include "MayaFlux/IO/Image/ImageWriter.hpp"
#include "MayaFlux/IO/StagedFileSet.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#include "MayaFlux/Transitive/Facet/ControlValueText.hpp"
#include "MayaFlux/Transitive/IO/JSONSerializer.hpp"

namespace MayaFlux::Nexus {

namespace {

    // -------------------------------------------------------------------------
    // Controls and the image
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

    std::map<std::string, std::string> controls_of(const QuantityStore& store)
    {
        std::map<std::string, std::string> held;
        store.for_each([&held](std::string_view name, const Quantity& slot) {
            if (!slot.has_value()) {
                return;
            }
            if (auto text = Reflect::control_text(slot); !text.empty()) {
                held.emplace(std::string(name), std::move(text));
            }
        });
        return held;
    }

    // -------------------------------------------------------------------------
    // Calls and names
    // -------------------------------------------------------------------------

    State::CallRecord call_record(const Reflect::FunctionCall& call)
    {
        return { .name = call.first, .arguments = { call.second.begin(), call.second.end() } };
    }

    template <typename Map, typename Object>
    std::string bound_name(const Map& names, const std::shared_ptr<Object>& object)
    {
        for (const auto& [name, held] : names) {
            if (held == object) {
                return name;
            }
        }
        return {};
    }

    /** The call of a function that exists, warned about when it has no name. */
    State::CallRecord named_call(const Reflect::FunctionCall& call, bool exists, std::string_view what, uint32_t id)
    {
        if (exists && call.first.empty()) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: {} {} has no name and is not encoded", what, id);
        }
        return call_record(call);
    }

    // -------------------------------------------------------------------------
    // Sink records
    // -------------------------------------------------------------------------

    void fill_sinks(
        State::EntityRecord& ent,
        const std::vector<AudioSink>& audio_sinks,
        const std::vector<RenderSink>& render_sinks,
        const State::WindowNames* windows)
    {
        for (const auto& s : audio_sinks)
            ent.audio_sinks.push_back({ .channel = s.channel, .fn_name = s.fn_name });

        for (const auto& s : render_sinks) {
            State::RenderSinkRecord rec { .fn_name = s.fn_name };
            if (windows && s.window) {
                rec.window = bound_name(*windows, s.window);
            }
            if (s.buf) {
                const auto config = s.buf->get_render_config();
                rec.vertex_shader = config.vertex_shader;
                rec.fragment_shader = config.fragment_shader;
                rec.geometry_shader = config.geometry_shader;
                rec.default_texture_binding = config.default_texture_binding;
                rec.topology = config.topology;
                rec.polygon_mode = config.polygon_mode;
                rec.cull_mode = config.cull_mode;
                rec.triangulate = config.triangulate;
                rec.draw_priority = config.draw_priority;
                rec.extra_string_params = config.extra_string_params;
            }
            ent.render_sinks.push_back(std::move(rec));
        }
    }

} // namespace

std::string StateEncoder::buffer_name(const std::shared_ptr<Buffers::VKBuffer>& buffer) const
{
    return m_bindings && buffer ? bound_name(m_bindings->buffers, buffer) : std::string {};
}

std::string StateEncoder::window_name(const std::shared_ptr<Core::Window>& window) const
{
    return m_bindings && window ? bound_name(m_bindings->windows, window) : std::string {};
}

std::string StateEncoder::source_name(const std::shared_ptr<Vruta::NetworkSource>& source) const
{
    return m_bindings && source ? bound_name(m_bindings->sources, source) : std::string {};
}

State::WiringRecord StateEncoder::build_wiring(const Fabric& fabric, uint32_t id) const
{
    State::WiringRecord rec;
    const Wiring* w = fabric.wiring_for(id);
    if (!w) {
        rec.lost.emplace_back("an unfinished wiring");
        return rec;
    }

    const auto keep = [&rec](const Reflect::FunctionCall& call, bool exists, std::string_view what) {
        if (exists && call.first.empty()) {
            rec.lost.emplace_back(what);
        }
        return call_record(call);
    };

    rec.interval = w->interval();
    rec.duration = w->duration();
    if (w->times_count() > 1) {
        rec.times = w->times_count();
    }
    for (const auto& step : w->move_steps()) {
        rec.steps.push_back({ .position = step.position, .delay_seconds = step.delay_seconds });
    }
    if (rec.interval || !rec.steps.empty()) {
        rec.interval_token = w->m_metro_token;
    }
    if (rec.duration) {
        rec.duration_token = w->m_duration_token;
    }

    if (w->event_factory()) {
        rec.factory_kind = State::FactoryKind::Event;
    } else if (std::holds_alternative<Wiring::SoundFactory>(w->factory())) {
        rec.factory_kind = State::FactoryKind::Sound;
    } else if (std::holds_alternative<Wiring::GraphicsFactory>(w->factory())) {
        rec.factory_kind = State::FactoryKind::Graphics;
    } else if (std::holds_alternative<Wiring::CrossFactory>(w->factory())) {
        rec.factory_kind = State::FactoryKind::Cross;
    }
    rec.factory = keep(w->factory_call(), rec.factory_kind != State::FactoryKind::None, "a factory");
    rec.place = keep(w->position_call(), w->has_position_fn(), "a placement source");

    rec.bound = w->has_bind();
    rec.bound_to_entity = w->m_bind_self;
    rec.bind_attach = keep(w->bind_attach_call(), rec.bound && !rec.bound_to_entity, "a bind function");
    rec.bind_detach = keep(w->bind_detach_call(), w->has_bind_detach(), "a bind detach function");
    rec.on_complete = keep(w->complete_call(), static_cast<bool>(w->m_on_complete), "an on_complete function");
    rec.on_detach = keep(w->detach_call(), static_cast<bool>(w->m_on_detach), "an on_detach function");

    const auto windowed = [&](State::TriggerRecord& trigger, const std::shared_ptr<Core::Window>& window) {
        trigger.source = window_name(window);
        if (trigger.source.empty()) {
            rec.lost.emplace_back("a trigger whose window has no bound name");
            return false;
        }
        return true;
    };

    for (const auto& held : w->triggers()) {
        std::visit([&](const auto& trigger) {
            using T = std::decay_t<decltype(trigger)>;
            State::TriggerRecord record;
            if constexpr (std::is_same_v<T, Wiring::KeyTrigger>) {
                record.kind = State::TriggerKind::Key;
                record.key = trigger.key.key;
                record.modifiers = trigger.key.modifiers;
                record.held = trigger.held;
                record.callback = keep(trigger.release_call, trigger.on_release.has_value(), "a key release function");
                if (windowed(record, trigger.window)) {
                    rec.triggers.push_back(std::move(record));
                }
            } else if constexpr (std::is_same_v<T, Wiring::MouseTrigger>) {
                record.kind = State::TriggerKind::Mouse;
                record.button = trigger.button;
                record.held = trigger.held;
                record.callback = keep(trigger.release_call, trigger.on_release.has_value(), "a mouse release function");
                if (windowed(record, trigger.window)) {
                    rec.triggers.push_back(std::move(record));
                }
            } else if constexpr (std::is_same_v<T, Wiring::ScrollTrigger>) {
                record.kind = State::TriggerKind::Scroll;
                record.callback = keep(trigger.call, true, "a scroll function");
                if (windowed(record, trigger.window)) {
                    rec.triggers.push_back(std::move(record));
                }
            } else if constexpr (std::is_same_v<T, Wiring::WindowEventTrigger>) {
                record.kind = State::TriggerKind::WindowEvent;
                record.event_type = trigger.filter.event_type;
                record.key = trigger.filter.key_code;
                record.button = trigger.filter.button;
                record.modifiers = trigger.filter.modifiers;
                if (windowed(record, trigger.window)) {
                    rec.triggers.push_back(std::move(record));
                }
            } else if constexpr (std::is_same_v<T, Wiring::NetworkTrigger>) {
                record.kind = State::TriggerKind::Network;
                record.source = source_name(trigger.source);
                if (record.source.empty()) {
                    rec.lost.emplace_back("a network trigger whose source has no bound name");
                } else {
                    rec.triggers.push_back(std::move(record));
                }
            } else if constexpr (std::is_same_v<T, Wiring::BroadcastTrigger>) {
                rec.lost.emplace_back("a broadcast trigger");
            }
        },
            held);
    }

    for (const auto& what : rec.lost) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateEncoder: entity {} loses {}", id, what);
    }
    return rec;
}

template <typename Entity>
void StateEncoder::fill_attachment(State::EntityRecord& record, const Entity& entity) const
{
    if (!entity.m_attachment) {
        return;
    }
    const auto name = buffer_name(entity.m_attachment->buf);
    if (name.empty()) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateEncoder: entity {} is attached to a buffer with no bound name", record.id);
        return;
    }
    const auto& config = entity.m_attachment->config;
    record.attach = State::AttachRecord {
        .buffer = name,
        .index = config.index,
        .offset = config.offset,
        .on_move = config.on_move,
    };
}

template <typename Record>
void StateEncoder::fill_expanse(Record& record, const Expanse& expanse) const
{
    record.contains = call_record(expanse.fn_call());
    record.on_enter = call_record(expanse.on_enter_fn_call());
    record.on_exit = call_record(expanse.on_exit_fn_call());
    if (const auto box = expanse.bounds()) {
        record.bounds_min = box->min;
        record.bounds_max = box->max;
    }

    std::shared_ptr<Buffers::VKBuffer> wrapped;
    if (const auto* hull = dynamic_cast<const Hull*>(&expanse)) {
        record.subkind = "hull";
        record.collection = hull->m_collection;
        wrapped = hull->m_buf;
    } else if (const auto* mantle = dynamic_cast<const Mantle*>(&expanse)) {
        record.subkind = "mantle";
        record.fit_name = Reflect::enum_to_lowercase_string(
            quantity_value<Portal::Graphics::FitMode>(mantle->m_mantle.fit)
                .value_or(Portal::Graphics::FitMode::STRETCH));
        wrapped = mantle->m_look.buf;
    }

    record.buffer = buffer_name(wrapped);
    if (wrapped && record.buffer.empty()) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateEncoder: {} wraps a buffer with no bound name", record.subkind);
    }
}

bool StateEncoder::encode(const Fabric& fabric, const std::string& base_path)
{
    return encode_fabric(fabric, base_path, {});
}

bool StateEncoder::encode_fabric(
    const Fabric& fabric,
    const std::string& base_path,
    const std::unordered_set<const Expanse*>& tapestry_owned,
    IO::StagedFileSet* staged)
{
    m_last_error.clear();

    // -------------------------------------------------------------------------
    // One record per entity, in id order. Its state is its controls.
    // -------------------------------------------------------------------------
    std::vector<State::EntityRecord> entities;

    auto ids = fabric.all_ids();
    std::ranges::sort(ids);

    for (uint32_t id : ids) {
        const auto k = fabric.kind(id);
        State::EntityRecord ent;
        ent.id = id;
        ent.kind = State::kind_to_string(k);
        ent.wiring = build_wiring(fabric, id);

        const auto* windows = m_bindings ? &m_bindings->windows : nullptr;

        switch (k) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(id);
            if (!e) {
                continue;
            }
            ent.controls = controls_of(e->store());
            ent.influence = named_call(e->fn_call(), static_cast<bool>(e->fn()), "the function of Emitter", id);
            fill_sinks(ent, e->audio_sinks(), e->render_sinks(), windows);
            fill_attachment(ent, *e);
            break;
        }
        case Fabric::Kind::Sensor: {
            auto s = fabric.get_sensor(id);
            if (!s) {
                continue;
            }
            ent.controls = controls_of(s->store());
            ent.perception = named_call(s->fn_call(), static_cast<bool>(s->fn()), "the function of Sensor", id);
            break;
        }
        case Fabric::Kind::Agent: {
            auto a = fabric.get_agent(id);
            if (!a) {
                continue;
            }
            ent.controls = controls_of(a->store());
            ent.influence = named_call(a->influence_fn_call(), static_cast<bool>(a->influence_fn()),
                "the influence function of Agent", id);
            ent.perception = named_call(a->perception_fn_call(), static_cast<bool>(a->perception_fn()),
                "the perception function of Agent", id);
            fill_sinks(ent, a->audio_sinks(), a->render_sinks(), windows);
            fill_attachment(ent, *a);

            if (auto locus = std::dynamic_pointer_cast<Locus>(a)) {
                ent.subkind = "locus";
                const auto& nav = locus->nav();
                ent.locus_nav = State::LocusNavRecord {
                    .eye = nav.eye,
                    .target = nav.eye + glm::vec3 { std::cos(nav.pitch) * std::sin(nav.yaw), std::sin(nav.pitch), std::cos(nav.pitch) * std::cos(nav.yaw) },
                    .fov = nav.fov_radians,
                    .near_plane = nav.near_plane,
                    .far_plane = nav.far_plane,
                    .speed = nav.move_speed,
                    .mouse_sensitivity = nav.mouse_sensitivity,
                    .scroll_speed = nav.scroll_speed,
                };
                for (const auto& proc : locus->view_targets()) {
                    std::string name;
                    if (m_bindings) {
                        for (const auto& [bound, buffer] : m_bindings->buffers) {
                            if (buffer && buffer->get_render_processor() == proc) {
                                name = bound;
                                break;
                            }
                        }
                    }
                    if (name.empty()) {
                        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                            "StateEncoder: Locus {} views a renderer of no bound buffer", id);
                    } else {
                        ent.view_targets.push_back(std::move(name));
                    }
                }
            } else if (auto presence = std::dynamic_pointer_cast<Presence>(a)) {
                ent.subkind = "presence";
                ent.radiate = named_call(presence->radiate_fn_call(), static_cast<bool>(presence->radiate_fn()),
                    "the radiate function of Presence", id);
                if (!presence->falloff_curve()) {
                    MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                        "StateEncoder: Presence {} has a custom falloff, which is not encoded", id);
                }
            }
            break;
        }
        }

        entities.push_back(std::move(ent));
    }

    // -------------------------------------------------------------------------
    // The picture: a column per entity, a value per numeric component of every
    // control any entity holds, ordered by control name.
    // -------------------------------------------------------------------------
    std::map<std::string, size_t> widths;
    std::vector<std::map<std::string, std::vector<double>>> numbers(entities.size());

    for (size_t i = 0; i < entities.size(); ++i) {
        for (const auto& [name, text] : entities[i].controls) {
            auto parsed = Reflect::parse_numbers<double>(text);
            if (!parsed || parsed->empty()) {
                continue;
            }
            const size_t held = widths.try_emplace(name, parsed->size()).first->second;
            if (held != parsed->size()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: entity {} holds control '{}' with {} numbers where another holds {}, left out of the image",
                    entities[i].id, name, parsed->size(), held);
                continue;
            }
            numbers[i].emplace(name, std::move(*parsed));
        }
    }

    std::vector<State::ComponentRecord> components;
    std::map<std::string, size_t> first_component;
    for (const auto& [name, width] : widths) {
        first_component.emplace(name, components.size());
        for (size_t c = 0; c < width; ++c) {
            components.push_back({ .name = name, .component = static_cast<uint32_t>(c) });
        }
    }

    std::vector<char> seen(components.size(), 0);
    for (const auto& column : numbers) {
        for (const auto& [name, values] : column) {
            const size_t base = first_component.at(name);
            for (size_t c = 0; c < values.size(); ++c) {
                bool initialized = seen[base + c] != 0;
                expand_range(components[base + c].range, static_cast<float>(values[c]), initialized);
                seen[base + c] = 1;
            }
        }
    }

    const auto width = static_cast<uint32_t>(entities.size());
    const auto rows = static_cast<uint32_t>(std::max<size_t>(1, (components.size() + State::k_channels - 1) / State::k_channels));

    IO::ImageData image;
    image.width = width;
    image.height = rows;
    image.channels = State::k_channels;
    image.format = Portal::Graphics::ImageFormat::RGBA32F;

    std::vector<float> pixels(static_cast<size_t>(width) * rows * State::k_channels, 0.0F);
    for (size_t i = 0; i < numbers.size(); ++i) {
        for (const auto& [name, values] : numbers[i]) {
            const size_t base = first_component.at(name);
            for (size_t c = 0; c < values.size(); ++c) {
                const size_t slot = base + c;
                const size_t row = slot / State::k_channels;
                const size_t channel = slot % State::k_channels;
                pixels[((row * width + i) * State::k_channels) + channel]
                    = normalize(static_cast<float>(values[c]), components[slot].range);
            }
        }
    }
    image.pixels = std::move(pixels);

    // -------------------------------------------------------------------------
    // Write EXR.
    // -------------------------------------------------------------------------
    const std::string exr_path = base_path + ".exr";
    const std::string json_path = base_path + ".json";

    const auto emit = [&](const std::string& path, const IO::StagedFileSet::WriteFn& writer) {
        bool written = false;
        if (staged == nullptr) {
            written = writer(path);
        } else {
            std::string error;
            written = staged->write(path, writer, error);
            if (!written && m_last_error.empty()) {
                m_last_error = error;
            }
        }
        if (!written) {
            MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        }
        return written;
    };

    if (!entities.empty()) {
        const bool exr_written = emit(exr_path, [&](const std::string& path) {
            auto writer = IO::ImageWriterRegistry::instance().create_writer(path);
            if (!writer) {
                m_last_error = "No ImageWriter registered for .exr";
                return false;
            }

            IO::ImageWriteOptions options;
            if (!writer->write(path, image, options)) {
                m_last_error = "EXR write failed: " + writer->get_last_error();
                return false;
            }
            return true;
        });
        if (!exr_written) {
            return false;
        }
    }

    // -------------------------------------------------------------------------
    // Build and write schema.
    // -------------------------------------------------------------------------
    State::FabricSchema schema;
    schema.version = State::k_schema_version;
    schema.fabric_name = fabric.name();
    schema.cell_size = fabric.m_cell_size;
    schema.components = std::move(components);
    schema.entities = std::move(entities);

    for (uint32_t xid : fabric.all_expanse_ids()) {
        const auto x = fabric.get_expanse(xid);
        if (!x || tapestry_owned.contains(x.get()))
            continue;
        State::ExpanseRecord record { .id = xid };
        fill_expanse(record, *x);
        if (record.subkind.empty() && record.contains.name.empty() && !record.bounds_min) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: Expanse {} has neither a named predicate nor bounds, skipping", xid);
            continue;
        }
        schema.expanses.push_back(std::move(record));
    }

    IO::JSONSerializer ser;
    const bool json_written = emit(json_path, [&](const std::string& path) {
        if (!ser.write(path, schema)) {
            m_last_error = "Failed to write schema: " + ser.last_error();
            return false;
        }
        return true;
    });
    if (!json_written) {
        return false;
    }

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} entities to {} + {}",
        schema.entities.size(), exr_path, json_path);

    return true;
}

bool StateEncoder::encode(const Tapestry& tapestry, const std::string& base_dir, nlohmann::json user_state)
{
    m_last_error.clear();

    IO::StagedFileSet set(base_dir);

    State::TapestrySchema schema;

    std::unordered_set<const Expanse*> tapestry_owned;
    for (const auto& [xname, xptr] : tapestry.all_expanses()) {
        tapestry_owned.insert(xptr.get());
    }

    for (const auto& fabric : tapestry.all_fabrics()) {
        const std::string fabric_id = fabric->name().empty()
            ? std::to_string(fabric->id())
            : fabric->name();

        if (!encode_fabric(*fabric, fabric_id, tapestry_owned, &set)) {
            return false;
        }

        schema.fabrics.push_back(State::FabricRef {
            .name = fabric_id,
            .base_path = (std::filesystem::path(set.directory()) / fabric_id).string(),
        });
    }

    for (const auto& [xname, xptr] : tapestry.all_expanses()) {
        State::TapestryExpanseRecord xrec { .name = xname };
        fill_expanse(xrec, *xptr);
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
    std::string error;
    const bool schema_staged = set.write(
        "tapestry.json",
        [&](const std::string& path) {
            if (!ser.write(path, schema)) {
                m_last_error = "Failed to write tapestry schema: " + ser.last_error();
                return false;
            }
            return true;
        },
        error);
    if (!schema_staged) {
        if (m_last_error.empty()) {
            m_last_error = error;
        }
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    if (!set.commit(m_last_error)) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO, m_last_error);
        return false;
    }

    const std::string tapestry_path = (std::filesystem::path(set.directory()) / "tapestry.json").string();

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} fabrics to {}", schema.fabrics.size(), tapestry_path);
    return true;
}

} // namespace MayaFlux::Nexus
