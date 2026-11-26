#include "Encoder.hpp"

#include "MayaFlux/Nexus/Pheme/Sinks.hpp"

#include "MayaFlux/Nexus/Expanses/Hull.hpp"
#include "MayaFlux/Nexus/Expanses/Mantle.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"
#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"
#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"

#include "MayaFlux/Nexus/Principals/Locus.hpp"
#include "MayaFlux/Nexus/Principals/Presence.hpp"

#include "MayaFlux/IO/Image/ImageExport.hpp"
#include "MayaFlux/IO/Image/ImageWriter.hpp"
#include "MayaFlux/IO/Model/ModelExport.hpp"
#include "MayaFlux/IO/Spatial/SpatialCache.hpp"
#include "MayaFlux/IO/Spatial/SpatialExport.hpp"
#include "MayaFlux/IO/StagedFileSet.hpp"
#include "MayaFlux/IO/Volume/VolumeTransfer.hpp"
#include "MayaFlux/Kakshya/NDData/VertexInsertion.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kriya/Course.hpp"

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

    std::vector<State::CallRecord> call_records(const std::vector<Reflect::FunctionCall>& calls)
    {
        std::vector<State::CallRecord> records;
        records.reserve(calls.size());
        for (const auto& call : calls) {
            records.push_back({ .name = call.first, .arguments = { call.second.begin(), call.second.end() } });
        }
        return records;
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

    template <typename Entity>
    void fill_sinks(
        State::EntityRecord& ent,
        const Entity& entity,
        const State::Bindings* bindings)
    {
        const auto* windows = bindings ? &bindings->windows : nullptr;
        for (const auto& s : entity.audio_sinks()) {
            if (!entity.made_by_configure(s.buf.get())) {
                ent.audio_sinks.push_back({ .channel = s.channel,
                    .fn_name = s.fn_name,
                    .arguments = { s.arguments.begin(), s.arguments.end() } });
            }
        }

        for (const auto& s : entity.render_sinks()) {
            if (entity.made_by_configure(s.buf.get())) {
                continue;
            }
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
            rec.arguments = { s.arguments.begin(), s.arguments.end() };
            for (const auto& image : s.images) {
                if (auto name = bindings ? bound_name(bindings->targets, image) : std::string {}; !name.empty()) {
                    rec.targets.push_back(std::move(name));
                } else {
                    MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                        "StateEncoder: entity {} draws into an image target with no bound name", ent.id);
                }
            }
            ent.render_sinks.push_back(std::move(rec));
        }
    }

    // -------------------------------------------------------------------------
    // Snapshots, one writer per kind of buffer, each answering the record without its name
    // -------------------------------------------------------------------------

    std::optional<State::AssetRecord> held(bool written, std::string file, State::AssetForm form)
    {
        if (!written) {
            return std::nullopt;
        }
        return State::AssetRecord { .file = std::move(file), .form = form };
    }

    /** One Alembic archive with a sample written by @p write. */
    std::optional<State::AssetRecord> snapshot_spatial(const std::filesystem::path& directory,
        const std::string& stem, const std::function<bool(IO::SpatialCache&)>& write)
    {
        const std::string file = stem + ".abc";
        IO::SpatialCache cache;
        if (!cache.open((directory / file).string())) {
            return std::nullopt;
        }
        const bool written = write(cache);
        cache.close();
        return held(written, file, State::AssetForm::Spatial);
    }

    void append_matrix(std::vector<float>& values, const glm::mat4& matrix)
    {
        for (glm::length_t column = 0; column < 4; ++column) {
            for (glm::length_t row = 0; row < 4; ++row) {
                values.push_back(matrix[column][row]);
            }
        }
    }

    /**
     * The template of an instance network as an assembly of one mesh, so its skin and morph
     * targets are written with it. A skin's joints become rows where the mesh was bound.
     */
    std::shared_ptr<Kakshya::MeshAssemblyContainer> template_assembly(
        const Kakshya::MeshData& mesh,
        const std::shared_ptr<const Kakshya::MeshSkin>& skin,
        const std::shared_ptr<const Kakshya::MeshMorphTargets>& morphs)
    {
        auto assembly = std::make_shared<Kakshya::MeshAssemblyContainer>();

        std::vector<uint64_t> joints;
        if (skin) {
            for (size_t j = 0; j < skin->joint_count(); ++j) {
                const auto row = assembly->add_placement(
                    { .name = "joint_" + std::to_string(j), .local = glm::inverse(skin->inverse_bind[j]) });
                if (!row) {
                    return nullptr;
                }
                joints.push_back(*row);
            }
        }

        Kakshya::MeshAssemblySource source { .geometry = mesh, .name = "template" };
        if (skin) {
            source.facets.set(skin);
        }
        if (morphs) {
            source.facets.set(Kakshya::MeshMorphTargets { *morphs });
        }
        const size_t held = assembly->add_source(std::move(source));

        std::optional<uint64_t> skeleton;
        if (skin) {
            skeleton = assembly->add_skeleton(std::move(joints));
        }
        if (!assembly->add_placement({ .name = "template", .source = held, .skeleton = skeleton })) {
            return nullptr;
        }
        return assembly;
    }

    std::optional<State::AssetRecord> snapshot_instances(
        const std::shared_ptr<Buffers::InstanceNetworkBuffer>& buffer,
        const std::filesystem::path& directory, const std::string& stem)
    {
        const auto network = buffer->get_network();
        if (!network || network->slot_count() == 0) {
            return std::nullopt;
        }
        const auto& first = network->get_slot(0);
        const auto node = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(first.node);
        const auto mesh = node ? node->get_mesh_data() : std::nullopt;
        if (!mesh) {
            return std::nullopt;
        }
        const auto assembly = template_assembly(*mesh, first.skin, first.morphs);
        if (!assembly) {
            return std::nullopt;
        }

        const std::string file = stem + ".glb";
        auto record = held(
            IO::save_mesh(std::shared_ptr<const Kakshya::MeshAssemblyContainer>(assembly), (directory / file).string()),
            file, State::AssetForm::Instances);
        if (!record) {
            return std::nullopt;
        }
        for (const auto& slot : network->slots()) {
            State::InstanceRecord instance {
                .name = slot.name,
                .tint = slot.tint,
                .scalar = slot.scalar,
                .texture = slot.texture,
                .morph_weights = slot.morph_weights,
            };
            append_matrix(instance.transform, slot.transform);
            for (const auto& joint : slot.palette) {
                append_matrix(instance.palette, joint);
            }
            record->instances.push_back(std::move(instance));
        }
        return record;
    }

    std::optional<State::AssetRecord> snapshot_texture(
        const std::shared_ptr<Buffers::TextureBuffer>& buffer,
        const std::filesystem::path& directory, const std::string& stem)
    {
        const auto image = IO::download_texture_buffer(buffer);
        if (!image) {
            return std::nullopt;
        }
        const std::string file = stem + (std::holds_alternative<std::vector<float>>(image->pixels) ? ".exr" : ".png");
        const std::string path = (directory / file).string();
        const auto writer = IO::ImageWriterRegistry::instance().create_writer(path);
        return held(writer && writer->write(path, *image, IO::ImageWriteOptions {}), file, State::AssetForm::Texture);
    }

    std::optional<State::AssetRecord> snapshot_nodes(
        const std::shared_ptr<Buffers::GeometryBuffer>& buffer,
        const std::filesystem::path& directory, const std::string& stem)
    {
        const auto node = buffer->get_geometry_node();
        const auto layout = node ? node->get_vertex_layout() : std::nullopt;
        if (!layout) {
            return std::nullopt;
        }

        std::vector<Kakshya::DataVariant> channels;
        Kakshya::VertexInsertion insertion(channels);
        if (!insertion.insert_interleaved(node->get_vertex_data(), *layout)) {
            return std::nullopt;
        }
        const auto at = std::ranges::find_if(layout->attributes, [](const auto& attribute) {
            return attribute.component_modality == Kakshya::DataModality::VERTEX_POSITIONS_3D;
        });
        if (at == layout->attributes.end()) {
            return std::nullopt;
        }
        const auto* positions = std::get_if<std::vector<glm::vec3>>(
            &channels[static_cast<size_t>(at - layout->attributes.begin())]);
        if (!positions || positions->empty()) {
            return std::nullopt;
        }

        const auto topology = buffer->get_render_config().topology;
        return snapshot_spatial(directory, stem, [&](IO::SpatialCache& cache) {
            return cache.write("geometry", IO::SpatialSample { .topology = topology, .positions = *positions });
        });
    }

    std::optional<State::AssetRecord> write_snapshot(
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        const std::filesystem::path& directory, const std::string& stem)
    {
        const std::string model = stem + ".glb";
        const std::string model_path = (directory / model).string();

        if (const auto mesh = std::dynamic_pointer_cast<Buffers::MeshBuffer>(buffer)) {
            return held(IO::save_mesh(mesh, model_path), model, State::AssetForm::Mesh);
        }
        if (const auto network = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buffer)) {
            return held(IO::save_mesh(network, model_path), model, State::AssetForm::Network);
        }
        if (const auto computed = std::dynamic_pointer_cast<Buffers::ComputeMeshBuffer>(buffer)) {
            return held(IO::save_mesh(computed, model_path), model, State::AssetForm::Mesh);
        }
        if (const auto instances = std::dynamic_pointer_cast<Buffers::InstanceNetworkBuffer>(buffer)) {
            return snapshot_instances(instances, directory, stem);
        }
        if (const auto volume = std::dynamic_pointer_cast<Buffers::VolumeGridBuffer>(buffer)) {
            const std::string file = stem + ".vdb";
            auto record = held(IO::save_volume(volume, (directory / file).string()), file, State::AssetForm::Volume);
            if (record) {
                record->resolution = { volume->get_width(), volume->get_height(), volume->get_depth() };
                record->bounds_min = volume->get_bounds().min;
                record->bounds_max = volume->get_bounds().max;
            }
            return record;
        }
        if (const auto texture = std::dynamic_pointer_cast<Buffers::TextureBuffer>(buffer)) {
            return snapshot_texture(texture, directory, stem);
        }
        if (const auto network = std::dynamic_pointer_cast<Buffers::NetworkGeometryBuffer>(buffer)) {
            return snapshot_spatial(directory, stem, [&](IO::SpatialCache& cache) {
                return IO::write_network_geometry_buffer_sample(cache, "geometry", network);
            });
        }
        if (const auto recorded = std::dynamic_pointer_cast<Buffers::RecordedGeometryBuffer>(buffer)) {
            const auto frame = recorded->current_frame();
            if (!frame) {
                return std::nullopt;
            }
            return snapshot_spatial(directory, stem, [&](IO::SpatialCache& cache) {
                return cache.write("geometry",
                    IO::SpatialSample { .topology = frame->topology,
                        .positions = frame->positions,
                        .vertex_counts_per_curve = frame->vertex_counts_per_curve,
                        .ids = frame->ids,
                        .velocities = frame->velocities,
                        .attributes = frame->attributes });
            });
        }
        if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buffer)) {
            return snapshot_nodes(geometry, directory, stem);
        }
        return std::nullopt;
    }

    /** The snapshot folders a schema's assets live in. */
    template <typename Schema>
    std::set<std::string> snapshot_folders(const std::string& json_path)
    {
        std::set<std::string> folders;
        std::error_code ec;
        if (!std::filesystem::exists(json_path, ec)) {
            return folders;
        }
        IO::JSONSerializer serializer;
        if (const auto previous = serializer.read<Schema>(json_path)) {
            for (const auto& asset : previous->assets) {
                const auto folder = std::filesystem::path(asset.file).parent_path().generic_string();
                if (asset.snapshot && folder.starts_with("assets") && folder.find('/') == std::string::npos) {
                    folders.insert(folder);
                }
            }
            for (const auto& motion : previous->motions) {
                const auto folder = std::filesystem::path(motion.file).parent_path().generic_string();
                if (motion.snapshot && folder.starts_with("assets") && folder.find('/') == std::string::npos) {
                    folders.insert(folder);
                }
            }
        }
        return folders;
    }

    /** Remove the folders an earlier save of the same state wrote and the new one no longer names. */
    void discard_folders(const std::filesystem::path& directory, const std::set<std::string>& earlier,
        const std::vector<State::AssetRecord>& current, const std::vector<State::MotionRecord>& motions)
    {
        for (const auto& folder : earlier) {
            const auto in_folder = [&folder](const auto& record) {
                return record.snapshot && std::filesystem::path(record.file).parent_path().generic_string() == folder;
            };
            if (!std::ranges::any_of(current, in_folder) && !std::ranges::any_of(motions, in_folder)) {
                std::error_code ec;
                std::filesystem::remove_all(directory / folder, ec);
            }
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

State::EntityRecord StateEncoder::sink_records(const Fabric& fabric, uint32_t id) const
{
    State::EntityRecord record;
    record.id = id;
    if (const auto emitter = fabric.get_emitter(id)) {
        fill_sinks(record, *emitter, m_bindings.get());
    } else if (const auto agent = fabric.get_agent(id)) {
        fill_sinks(record, *agent, m_bindings.get());
    }
    return record;
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
    rec.configurations = call_records(w->m_configurations);
    for (size_t i = 0; i < w->move_steps().size(); ++i) {
        if (std::ranges::find(w->m_configured_steps, i) == w->m_configured_steps.end()) {
            const auto& step = w->move_steps()[i];
            rec.steps.push_back({ .position = step.position, .delay_seconds = step.delay_seconds });
        }
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

    for (size_t index = 0; index < w->triggers().size(); ++index) {
        if (std::ranges::find(w->m_configured_triggers, index) != w->m_configured_triggers.end()) {
            continue;
        }
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
                record.kind = State::TriggerKind::Course;
                if (m_bindings) {
                    for (const auto& [name, course] : m_bindings->courses) {
                        if (const auto kind = course ? course->subscription(trigger.source.get()) : std::nullopt) {
                            record.source = name;
                            record.course_event = kind;
                        }
                    }
                }
                if (record.source.empty()) {
                    rec.lost.emplace_back("a broadcast trigger that is not a bound course's event");
                } else {
                    if (trigger.receives) {
                        rec.lost.emplace_back("the receive function of a course trigger");
                    }
                    rec.triggers.push_back(std::move(record));
                }
            }
        },
            w->triggers()[index]);
    }

    for (const auto& what : rec.lost) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateEncoder: entity {} loses {}", id, what);
    }
    return rec;
}

std::vector<State::AssetRecord> StateEncoder::save_assets(const std::string& directory) const
{
    std::vector<State::AssetRecord> records;
    if (!m_bindings) {
        return records;
    }

    namespace fs = std::filesystem;
    std::string folder;
    const auto snapshot_folder = [&]() -> const std::string& {
        if (folder.empty()) {
            folder = "assets";
            for (uint32_t n = 1; fs::exists(fs::path(directory) / folder); ++n) {
                folder = "assets_" + std::to_string(n);
            }
            std::error_code ec;
            fs::create_directories(fs::path(directory) / folder, ec);
        }
        return folder;
    };

    for (const auto& [name, buffer] : m_bindings->buffers) {
        if (const auto declared = m_bindings->assets.find(name); declared != m_bindings->assets.end()) {
            const auto& source = declared->second;
            if (!source.make.first.empty()) {
                records.push_back({
                    .name = name,
                    .form = State::AssetForm::Call,
                    .make = call_record(source.make),
                });
                continue;
            }
            records.push_back({
                .name = name,
                .file = source.file,
                .form = source.row ? State::AssetForm::Part : State::AssetForm::Mesh,
                .index = source.row.value_or(source.mesh),
            });
            records.back().realize = call_record(source.realize);
            if (source.realize_bounds) {
                records.back().bounds_min = source.realize_bounds->min;
                records.back().bounds_max = source.realize_bounds->max;
            }
            continue;
        }

        const bool plain = name.find_first_of("/\\") == std::string::npos && name != "." && name != "..";
        const std::string stem = snapshot_folder() + "/"
            + (plain ? name : "asset_" + std::to_string(records.size()));
        if (auto record = write_snapshot(buffer, directory, stem)) {
            record->name = name;
            record->snapshot = true;
            records.push_back(std::move(*record));
        } else {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: buffer '{}' could not be written as a snapshot", name);
        }
    }
    return records;
}

std::vector<State::MotionRecord> StateEncoder::save_motions(
    const std::string& directory, const std::vector<std::pair<std::string, const Fabric*>>& fabrics) const
{
    std::vector<State::MotionRecord> records;
    if (!m_bindings) {
        return records;
    }

    namespace fs = std::filesystem;
    std::string folder;
    for (const auto& [name, source] : m_bindings->motions) {
        State::MotionRecord record {
            .name = name,
            .file = source.file,
            .motion = source.motion,
            .edge = source.edge,
            .courses = source.courses,
            .buffers = source.buffers,
            .rows = source.rows,
        };
        record.realize = call_record(source.realize);
        if (source.realize_bounds) {
            record.bounds_min = source.realize_bounds->min;
            record.bounds_max = source.realize_bounds->max;
        }

        if (record.file.empty()) {
            if (folder.empty()) {
                folder = "assets";
                for (uint32_t n = 1; fs::exists(fs::path(directory) / folder); ++n) {
                    folder = "assets_" + std::to_string(n);
                }
                std::error_code ec;
                fs::create_directories(fs::path(directory) / folder, ec);
            }
            record.file = folder + "/" + name + ".glb";
            record.snapshot = true;
            if (!source.assembly || !IO::save_mesh(source.assembly, (fs::path(directory) / record.file).string())) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: the assembly of motion '{}' could not be written, so it is not saved", name);
                continue;
            }
        }

        for (const auto& [row, member] : source.carried) {
            std::visit([&](const auto& weak) {
                const auto live = weak.lock();
                if (!live) {
                    return;
                }
                for (const auto& [label, fabric] : fabrics) {
                    using Principal = typename std::decay_t<decltype(live)>::element_type;
                    bool here = false;
                    if constexpr (std::is_same_v<Principal, Emitter>) {
                        here = fabric->get_emitter(live->id()) == live;
                    } else if constexpr (std::is_same_v<Principal, Sensor>) {
                        here = fabric->get_sensor(live->id()) == live;
                    } else {
                        here = fabric->get_agent(live->id()) == live;
                    }
                    if (here) {
                        record.carried.push_back({ .row = row, .fabric = label, .entity = live->id() });
                        return;
                    }
                }
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: motion '{}' carries a principal of no saved Fabric", name);
            },
                member);
        }
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<State::CourseRecord> StateEncoder::save_courses() const
{
    std::vector<State::CourseRecord> records;
    if (!m_bindings) {
        return records;
    }

    for (const auto& [name, course] : m_bindings->courses) {
        if (!course) {
            continue;
        }
        State::CourseRecord record {
            .name = name,
            .time = course->time(),
            .paused = course->paused(),
        };
        record.step_rate = course->step_rate();
        if (course->advanced()) {
            record.playback = course->playback();
        }
        if (const auto followed = course->following()) {
            for (const auto& [other, held] : m_bindings->courses) {
                if (held == followed->first) {
                    record.follows = other;
                    record.follow_offset = followed->second;
                }
            }
        }
        course->store().for_each([&record](std::string_view control, const ControlValue& slot) {
            if (!slot.has_value()) {
                return;
            }
            if (auto text = Reflect::control_text(slot); !text.empty()) {
                record.controls.emplace(std::string(control), std::move(text));
            }
        });

        const auto held = course->get<Kriya::CourseControls>();
        if (const auto range = held.range.value()) {
            record.range_in = range->in;
            record.range_out = range->out;
        }
        record.configurations = call_records(course->configurations());
        if (record.configurations.empty()) {
            if (const auto map = held.time_map.value(); map && map->fn && record.follows.empty()) {
                record.lost.emplace_back("its time map");
            }
            if (const auto map = held.repeat.value(); map && map->fn) {
                record.lost.emplace_back("its repeat");
            }
        }
        for (const auto& marker : course->markers()) {
            record.markers.push_back({ .name = marker.name, .time = marker.time });
        }
        for (const auto& what : record.lost) {
            MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateEncoder: course '{}' loses {}", name, what);
        }
        records.push_back(std::move(record));
    }
    return records;
}

std::optional<State::ViewSinkRecord> StateEncoder::sink_of(
    const Fabric& fabric, const std::shared_ptr<Buffers::RenderProcessor>& renderer) const
{
    const auto among = [&renderer](uint32_t id, const auto& entity) -> std::optional<State::ViewSinkRecord> {
        uint32_t place = 0;
        for (const auto& sink : entity->render_sinks()) {
            if (entity->made_by_configure(sink.buf.get())) {
                continue;
            }
            if (sink.renderer == renderer) {
                return State::ViewSinkRecord { .entity = id, .sink = place };
            }
            ++place;
        }
        return std::nullopt;
    };

    for (const uint32_t id : fabric.all_ids()) {
        if (const auto emitter = fabric.get_emitter(id)) {
            if (auto found = among(id, emitter)) {
                return found;
            }
        } else if (const auto agent = fabric.get_agent(id)) {
            if (auto found = among(id, agent)) {
                return found;
            }
        }
    }
    return std::nullopt;
}

std::string StateEncoder::class_name(const Agent& agent) const
{
    if (m_bindings) {
        for (const auto& [name, registered] : m_bindings->agents) {
            if (registered.type == std::type_index(typeid(agent))) {
                return name;
            }
        }
    }
    return {};
}

std::string StateEncoder::class_name(const Expanse& expanse) const
{
    if (m_bindings) {
        for (const auto& [name, registered] : m_bindings->expanses) {
            if (registered.type == std::type_index(typeid(expanse))) {
                return name;
            }
        }
    }
    return {};
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
        .slot = config.slot,
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

    if (auto registered = class_name(expanse); !registered.empty()) {
        record.subkind = std::move(registered);
        wrapped = nullptr;
    }
    record.configurations = call_records(expanse.configurations());
    if ((!expanse.m_perceptions.empty() || !expanse.m_entries.empty() || !expanse.m_exits.empty()
            || !expanse.m_insides.empty())
        && expanse.configurations().empty()) {
        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateEncoder: typed perceptions or actions of an Expanse were not added through configure() and are lost");
    }
    record.controls = controls_of(expanse.store());
    record.controls.erase("bounds");

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

        switch (k) {
        case Fabric::Kind::Emitter: {
            auto e = fabric.get_emitter(id);
            if (!e) {
                continue;
            }
            ent.controls = controls_of(e->store());
            ent.influence = named_call(e->fn_call(), static_cast<bool>(e->fn()), "the function of Emitter", id);
            fill_sinks(ent, *e, m_bindings.get());
            ent.configurations = call_records(e->configurations());
            if (e->m_influence && e->configurations().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: the typed influence of Emitter {} was not set through configure() and is lost", id);
            }
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
            ent.configurations = call_records(s->configurations());
            if (s->m_perception && s->configurations().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: the typed perception of Sensor {} was not set through configure() and is lost", id);
            }
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
            fill_sinks(ent, *a, m_bindings.get());
            ent.configurations = call_records(a->configurations());
            if ((!a->m_influences.empty() || !a->m_perceptions.empty()) && a->configurations().empty()) {
                MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                    "StateEncoder: typed influences or perceptions of Agent {} were not added through configure() and are lost", id);
            }
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
                    if (!name.empty()) {
                        ent.view_targets.push_back(std::move(name));
                    } else if (const auto sink = sink_of(fabric, proc)) {
                        ent.view_sinks.push_back(*sink);
                    } else {
                        MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                            "StateEncoder: Locus {} views a renderer of no bound buffer or entity sink", id);
                    }
                }
            } else if (auto presence = std::dynamic_pointer_cast<Presence>(a)) {
                ent.subkind = "presence";
                ent.radiate = named_call(presence->radiate_fn_call(), static_cast<bool>(presence->radiate_fn()),
                    "the radiate function of Presence", id);
                if (!presence->falloff_curve() && presence->configurations().empty()) {
                    MF_WARN(Journal::Component::Nexus, Journal::Context::FileIO,
                        "StateEncoder: Presence {} has a custom falloff, which is not encoded", id);
                }
            }
            if (auto registered = class_name(*a); !registered.empty()) {
                ent.subkind = std::move(registered);
            }
            if (auto locus = std::dynamic_pointer_cast<Locus>(a); locus && locus->view()) {
                for (const uint32_t earlier : ids) {
                    if (earlier == id) {
                        break;
                    }
                    if (fabric.kind(earlier) != Fabric::Kind::Agent) {
                        continue;
                    }
                    const auto other = std::dynamic_pointer_cast<Locus>(fabric.get_agent(earlier));
                    if (other && other->view() == locus->view()) {
                        ent.view_of = earlier;
                        break;
                    }
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
    const std::filesystem::path written_to = staged ? std::string {} : IO::resolve_write_path(json_path);
    const auto earlier = staged ? std::set<std::string> {} : snapshot_folders<State::FabricSchema>(written_to.string());

    State::FabricSchema schema;
    schema.version = State::k_schema_version;
    schema.fabric_name = fabric.name();
    schema.cell_size = fabric.m_cell_size;
    if (staged == nullptr) {
        schema.assets = save_assets(std::filesystem::path(IO::resolve_write_path(json_path)).parent_path().string());
        schema.courses = save_courses();
        schema.motions = save_motions(written_to.parent_path().string(), { { std::string {}, &fabric } });
    }
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
    if (!staged) {
        discard_folders(written_to.parent_path(), earlier, schema.assets, schema.motions);
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
    const auto earlier = snapshot_folders<State::TapestrySchema>(
        (std::filesystem::path(set.directory()) / "tapestry.json").string());

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
    schema.assets = save_assets(set.directory());
    schema.courses = save_courses();
    std::vector<std::pair<std::string, const Fabric*>> saved;
    for (const auto& fabric : tapestry.all_fabrics()) {
        saved.emplace_back(fabric->name().empty() ? std::to_string(fabric->id()) : fabric->name(), fabric.get());
    }
    schema.motions = save_motions(set.directory(), saved);

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
    discard_folders(set.directory(), earlier, schema.assets, schema.motions);

    MF_INFO(Journal::Component::Nexus, Journal::Context::FileIO,
        "StateEncoder: wrote {} fabrics to {}", schema.fabrics.size(), tapestry_path);
    return true;
}

} // namespace MayaFlux::Nexus
