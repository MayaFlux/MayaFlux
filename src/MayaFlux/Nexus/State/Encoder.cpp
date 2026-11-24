#include "Encoder.hpp"

#include "MayaFlux/Nexus/Pheme/Sinks.hpp"

#include "MayaFlux/Nexus/Expanses/Hull.hpp"
#include "MayaFlux/Nexus/Expanses/Mantle.hpp"

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
    // Sink records
    // -------------------------------------------------------------------------

    void fill_sinks(
        State::EntityRecord& ent,
        const std::vector<AudioSink>& audio_sinks,
        const std::vector<RenderSink>& render_sinks)
    {
        for (const auto& s : audio_sinks)
            ent.audio_sinks.push_back({ .channel = s.channel, .fn_name = s.fn_name });

        for (const auto& s : render_sinks) {
            State::RenderSinkRecord rec { .fn_name = s.fn_name };
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

State::WiringRecord StateEncoder::build_wiring(const Fabric& fabric, uint32_t id)
{
    const Wiring* w = fabric.wiring_for(id);
    if (!w)
        return { .kind = State::WiringKind::Unsupported };

    State::WiringRecord rec;
    rec.position_fn_name = w->position_fn_name();

    const auto take_times = [&] {
        if (w->times_count() > 1)
            rec.times = w->times_count();
    };
    const auto take_duration = [&] {
        if (w->duration()) {
            rec.duration = w->duration();
            rec.duration_token = w->m_duration_token;
        }
    };

    if (!std::holds_alternative<std::monostate>(w->factory()) || w->event_factory()) {
        rec.kind = State::WiringKind::Factory;
        rec.factory_name = w->factory_name();

    } else if (w->has_bind()) {
        rec.kind = State::WiringKind::Bind;
        rec.attach_fn_name = w->bind_attach_name();
        rec.detach_fn_name = w->bind_detach_name();
        take_duration();

    } else if (!w->move_steps().empty()) {
        rec.kind = State::WiringKind::MoveTo;
        std::vector<State::WiringStep> steps;
        steps.reserve(w->move_steps().size());
        for (const auto& s : w->move_steps())
            steps.push_back({ .position = s.position, .delay_seconds = s.delay_seconds });
        rec.steps = std::move(steps);
        take_times();

    } else if (!w->triggers().empty()) {
        if (w->triggers().size() > 1) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: entity {} has {} triggers, only the first is encoded", id, w->triggers().size());
        }
        std::visit([&rec](const auto& trigger) {
            using T = std::decay_t<decltype(trigger)>;
            if constexpr (std::is_same_v<T, Wiring::KeyTrigger>) {
                rec.kind = State::WiringKind::Key;
                rec.key = trigger.key.key;
                rec.key_modifiers = trigger.key.modifiers;
                rec.held = trigger.held;
            } else if constexpr (std::is_same_v<T, Wiring::MouseTrigger>) {
                rec.kind = State::WiringKind::Mouse;
                rec.button = trigger.button;
                rec.held = trigger.held;
            } else if constexpr (std::is_same_v<T, Wiring::ScrollTrigger>) {
                rec.kind = State::WiringKind::Scroll;
            } else if constexpr (std::is_same_v<T, Wiring::NetworkTrigger>) {
                rec.kind = State::WiringKind::Network;
            } else if constexpr (std::is_same_v<T, Wiring::WindowEventTrigger>) {
                rec.kind = State::WiringKind::WindowEvent;
            } else {
                rec.kind = State::WiringKind::Unsupported;
            }
        },
            w->triggers().front());

    } else if (w->interval()) {
        rec.kind = State::WiringKind::Every;
        rec.interval = w->interval();
        rec.interval_token = w->m_metro_token;
        take_duration();
        take_times();
    }

    return rec;
}

template <typename Record>
void StateEncoder::fill_expanse(Record& record, const Expanse& expanse)
{
    record.fn_name = expanse.fn_name();
    record.on_enter_fn_name = expanse.on_enter_fn_name();
    record.on_exit_fn_name = expanse.on_exit_fn_name();
    if (const auto box = expanse.bounds()) {
        record.bounds_min = box->min;
        record.bounds_max = box->max;
    }

    if (const auto* hull = dynamic_cast<const Hull*>(&expanse)) {
        record.subkind = "hull";
        record.collection = hull->m_collection;
    } else if (const auto* mantle = dynamic_cast<const Mantle*>(&expanse)) {
        record.subkind = "mantle";
        record.fit_name = Reflect::enum_to_lowercase_string(
            quantity_value<Portal::Graphics::FitMode>(mantle->m_mantle.fit)
                .value_or(Portal::Graphics::FitMode::STRETCH));
    }

    if (!record.subkind.empty()) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateEncoder: {} wraps a live buffer that is not encoded", record.subkind);
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

        switch (k) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(id);
            if (!e) {
                continue;
            }
            if (e->fn() && e->fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Emitter {} has a function but no fn_name", id);
            }
            ent.controls = controls_of(e->store());
            ent.influence_fn_name = e->fn_name();
            fill_sinks(ent, e->audio_sinks(), e->render_sinks());
            break;
        }
        case Fabric::Kind::Sensor: {
            auto s = fabric.get_sensor(id);
            if (!s) {
                continue;
            }
            if (s->fn() && s->fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Sensor {} has a function but no fn_name", id);
            }
            ent.controls = controls_of(s->store());
            ent.perception_fn_name = s->fn_name();
            break;
        }
        case Fabric::Kind::Agent: {
            auto a = fabric.get_agent(id);
            if (!a) {
                continue;
            }
            if (a->perception_fn() && a->perception_fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Agent {} has a perception function but no perception_fn_name", id);
            }
            if (a->influence_fn() && a->influence_fn_name().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: Agent {} has an influence function but no influence_fn_name", id);
            }
            ent.controls = controls_of(a->store());
            ent.influence_fn_name = a->influence_fn_name();
            ent.perception_fn_name = a->perception_fn_name();
            fill_sinks(ent, a->audio_sinks(), a->render_sinks());

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
            } else if (auto presence = std::dynamic_pointer_cast<Presence>(a)) {
                ent.subkind = "presence";
                ent.radiate_fn_name = presence->radiate_fn_name();
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
        if (record.subkind.empty() && record.fn_name.empty() && !record.bounds_min) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: Expanse {} has neither fn_name nor bounds, skipping", xid);
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
