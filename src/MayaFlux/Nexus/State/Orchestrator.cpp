#include "Orchestrator.hpp"

#include "AssemblySource.hpp"

#include "MayaFlux/Nexus/Pheme/Survey.hpp"

#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"
#include "MayaFlux/IO/Detail/IOTransitions.hpp"
#include "MayaFlux/IO/Model/ModelReader.hpp"
#include "MayaFlux/IO/Spatial/SpatialAssembly.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyMotionFrames.hpp"
#include "MayaFlux/Kinesis/MatrixTransforms.hpp"

#include <glm/gtc/quaternion.hpp>

namespace MayaFlux::Nexus {

namespace Detail {

    std::shared_ptr<Kakshya::MeshAssemblyContainer> read_assembly(const std::string& filepath)
    {
        auto extension = std::filesystem::path(filepath).extension().string();
        std::ranges::transform(extension, extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".abc") {
            return IO::open_spatial_assembly(filepath);
        }

        auto reader = std::make_shared<IO::ModelReader>();
        if (!reader->can_read(filepath) || !reader->open(filepath)) {
            MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO,
                "StateOrchestrator: cannot read '{}': {}", filepath, reader->get_last_error());
            return nullptr;
        }

        auto assembly = reader->create_mesh_assembly();
        reader->close();
        return assembly;
    }

}

namespace {

    /** Position and orientation of a matrix, empty where it has no scale on some axis. */
    PlacementControls placement_of(const glm::mat4& matrix)
    {
        PlacementControls placement;
        if (const auto parts = Kinesis::decompose_transform(matrix)) {
            placement.position = std::get<0>(*parts);
            placement.orientation = std::get<1>(*parts);
        }
        return placement;
    }

    /** Apply the members of @p incoming the principal has not been given. */
    void fill(const Fabric::Member& member, const PlacementControls& incoming)
    {
        std::visit([&incoming](const auto& ptr) {
            PlacementControls missing;
            if (!ptr->position()) {
                missing.position = incoming.position;
            }
            if (!ptr->orientation()) {
                missing.orientation = incoming.orientation;
            }
            ptr->set(missing);
        },
            member);
    }

    std::shared_ptr<Locus> as_locus(const Fabric::Member& member)
    {
        const auto* agent = std::get_if<std::shared_ptr<Agent>>(&member);
        return agent ? std::dynamic_pointer_cast<Locus>(*agent) : nullptr;
    }

    /** Alembic through its archive, so animated sources keep their frames; anything else as a model. */
    std::shared_ptr<Locus> new_locus()
    {
        return std::make_shared<Locus>(
            Kinesis::NavigationConfig {}, 1.0F, Agent::PerceptionFn {}, Agent::InfluenceFn {});
    }

    void look_from(const std::shared_ptr<Locus>& locus, const Kinesis::Viewpoint& view)
    {
        Kinesis::set_viewpoint(locus->nav(), view);
        const auto taken = Kinesis::viewpoint_of(locus->nav());
        locus->view()->write(locus->driver(), taken);
        locus->set({ .position = glm::vec3(taken.first[3]),
            .orientation = glm::normalize(glm::quat_cast(glm::mat3(taken.first))) });
    }

    /** Look out from a row's placement, keeping the Locus's frustum. */
    void look_from_row(const std::shared_ptr<Locus>& locus, const glm::mat4& world)
    {
        if (const auto parts = Kinesis::decompose_transform(world)) {
            const auto rigid = Kinesis::compose_transform(
                std::get<0>(*parts), std::get<1>(*parts), glm::vec3(1.0F));
            look_from(locus, { rigid, Kinesis::viewpoint_of(locus->nav()).second });
        }
    }

    void take_lens(
        const std::shared_ptr<Locus>& locus,
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
        size_t row)
    {
        const auto placed = assembly->placement(row);
        if (!placed || !placed->source) {
            return;
        }
        if (const auto* lens = assembly->lens(*placed->source)) {
            look_from(locus, { Kinesis::viewpoint_of(locus->nav()).first, Kakshya::frustum_of(*lens) });
        }
    }

    void take_color(
        const std::shared_ptr<Emitter>& emitter,
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
        size_t row)
    {
        const auto placed = assembly->placement(row);
        if (!placed || !placed->source) {
            return;
        }
        if (const auto* light = assembly->light(*placed->source)) {
            emitter->set(EmitterControls { .color = light->color });
        }
    }

}

// =============================================================================
// Pieces
// =============================================================================

std::shared_ptr<Locus> StateOrchestrator::locus_from(const Kinesis::Viewpoint& view)
{
    auto locus = new_locus();
    look_from(locus, view);
    admit(Fabric::Member { std::shared_ptr<Agent>(locus) });
    return locus;
}

std::shared_ptr<Locus> StateOrchestrator::locus_from(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row)
{
    if (!assembly) {
        return nullptr;
    }
    const auto world = assembly->world_transforms();
    if (row >= world.size()) {
        return nullptr;
    }

    auto locus = new_locus();
    take_lens(locus, assembly, row);
    look_from_row(locus, world[row]);
    admit(Fabric::Member { std::shared_ptr<Agent>(locus) }, {}, {}, row);
    return locus;
}

std::shared_ptr<Emitter> StateOrchestrator::emitter_from(
    const Kakshya::MeshAssemblyLight& light, const glm::mat4& placement)
{
    auto emitter = std::make_shared<Emitter>();
    emitter->set(EmitterControls { .color = light.color });
    emitter->set(placement_of(placement));
    admit(Fabric::Member { emitter });
    return emitter;
}

std::shared_ptr<Emitter> StateOrchestrator::emitter_from(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row)
{
    if (!assembly) {
        return nullptr;
    }
    const auto world = assembly->world_transforms();
    if (row >= world.size()) {
        return nullptr;
    }

    auto emitter = std::make_shared<Emitter>();
    take_color(emitter, assembly, row);
    emitter->set(placement_of(world[row]));
    admit(Fabric::Member { emitter }, {}, {}, row);
    return emitter;
}

Wiring::PlacementFn StateOrchestrator::placement_from(
    const std::shared_ptr<Buffers::VKBuffer>& buffer, std::optional<uint32_t> slot)
{
    return [held = std::weak_ptr<Buffers::VKBuffer>(buffer), slot,
               anchor = track_anchor(buffer, slot)]() -> PlacementControls {
        const auto live = held.lock();
        const auto at = live ? anchor() : std::nullopt;
        if (!at) {
            return {};
        }

        const glm::mat4 drawn = read_placement(live, slot);
        PlacementControls placement = placement_of(drawn);
        placement.position = glm::vec3(drawn * glm::vec4(*at, 1.0F));
        return placement;
    };
}

StateOrchestrator::BufferSlot StateOrchestrator::buffer_of(
    const std::shared_ptr<const Kriya::MeshAssemblyRealization>& realization, uint64_t row)
{
    if (!realization) {
        return {};
    }

    for (const auto& part : realization->parts()) {
        const auto found = std::ranges::find(part.rows, row);
        if (found == part.rows.end()) {
            continue;
        }
        const auto index = static_cast<size_t>(found - part.rows.begin());

        if (part.network_buffer) {
            return { part.network_buffer, static_cast<uint32_t>(index) };
        }
        if (part.instance_buffer) {
            return { part.instance_buffer, static_cast<uint32_t>(index) };
        }
        if (index < part.spatial_buffers.size() && part.spatial_buffers[index]) {
            return { part.spatial_buffers[index], std::nullopt };
        }
    }
    return {};
}

StateOrchestrator::Reflows StateOrchestrator::motion_from(
    const std::shared_ptr<const Kriya::MeshAssemblyRealization>& realization,
    size_t motion,
    Kriya::CourseEdge edge,
    bool paused) const
{
    Reflows reflows;
    if (!realization || !m_scheduler) {
        return reflows;
    }

    auto& [placing, deforming, streaming] = reflows;
    const auto& assembly = realization->assembly();

    if (motion < assembly->get_data().motions.size()) {
        if (const auto frames = Kakshya::MeshAssemblyMotionFrames::create(assembly, motion)) {
            Kriya::CourseSetting setting;
            setting.in = frames->extent().first;
            setting.out = frames->extent().second;
            setting.edge = edge;
            placing = Kriya::MotionReflowBuilder::sampling(frames, realization->placement_sink(), 0.0, m_scheduler)
                          .course(setting, paused)
                          .start();
        }
    }

    for (auto& route : realization->mesh_routes(edge)) {
        route.paused = paused;
        if (auto reflow = Kriya::MeshReflowBuilder(nullptr, {}, 0.0, m_scheduler).route(std::move(route)).start()) {
            deforming.push_back(std::move(reflow));
        }
    }

    for (auto& route : realization->spatial_routes(edge)) {
        route.paused = paused;
        if (auto reflow = Kriya::SpatialReflowBuilder(nullptr, {}, 0.0, m_scheduler).route(std::move(route)).start()) {
            streaming.push_back(std::move(reflow));
        }
    }
    return reflows;
}

// =============================================================================
// Whole assembly
// =============================================================================

Fabric::Member StateOrchestrator::default_row(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row)
{
    if (!assembly) {
        return {};
    }

    switch (Kakshya::row_kind(*assembly, row)) {
    case Kakshya::MeshAssemblyRowKind::Camera: {
        auto locus = new_locus();
        take_lens(locus, assembly, row);
        return std::shared_ptr<Agent>(std::move(locus));
    }

    case Kakshya::MeshAssemblyRowKind::Light: {
        auto emitter = std::make_shared<Emitter>();
        take_color(emitter, assembly, row);
        return emitter;
    }

    case Kakshya::MeshAssemblyRowKind::Mesh:
    case Kakshya::MeshAssemblyRowKind::Points:
    case Kakshya::MeshAssemblyRowKind::Curves:
        return std::make_shared<Agent>();

    case Kakshya::MeshAssemblyRowKind::Joint:
        break;
    }
    return {};
}

std::pair<std::shared_ptr<Kriya::MeshAssemblyRealization>, StateOrchestrator::Rows>
StateOrchestrator::orchestrate(const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly)
{
    return orchestrate_rows(assembly, m_material_resolver);
}

std::pair<std::shared_ptr<Kriya::MeshAssemblyRealization>, StateOrchestrator::Rows>
StateOrchestrator::orchestrate_rows(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    const Kriya::MeshAssemblyMaterialResolver& materials,
    const std::string& file)
{
    if (!assembly) {
        return {};
    }

    auto realization = Kriya::MeshAssemblyRealization::create(assembly, m_realize_options);
    if (realization && materials) {
        realization->bind_materials(materials);
    }
    Rows rows;

    const auto world = assembly->world_transforms();
    for (size_t row = 0; row < world.size(); ++row) {
        auto member = m_row_rule ? m_row_rule(assembly, row) : default_row(assembly, row);
        if (!std::visit([](const auto& ptr) { return ptr != nullptr; }, member)) {
            continue;
        }

        const bool wired = std::visit([](const auto& ptr) { return ptr->id() != 0; }, member);
        if (!wired) {
            const auto shown = buffer_of(realization, row);
            if (const auto locus = as_locus(member)) {
                look_from_row(locus, world[row]);
                admit(member, {}, {}, row);
            } else if (shown.first) {
                wrap(member, shown, file, row);
                fill(member, placement_of(world[row]));
            } else {
                fill(member, placement_of(world[row]));
                admit(member, {}, {}, row);
            }
        }

        rows.emplace_back(row, std::move(member));
    }
    return { std::move(realization), std::move(rows) };
}

std::tuple<
    std::shared_ptr<Kakshya::MeshAssemblyContainer>,
    std::shared_ptr<Kriya::MeshAssemblyRealization>,
    StateOrchestrator::Rows>
StateOrchestrator::orchestrate(const std::string& filepath)
{
    auto assembly = Detail::read_assembly(filepath);
    if (!assembly) {
        MF_ERROR(Journal::Component::Nexus, Journal::Context::FileIO,
            "StateOrchestrator: no assembly from '{}'", filepath);
        return {};
    }

    auto [realization, rows] = orchestrate_rows(assembly,
        m_material_resolver ? m_material_resolver : IO::Detail::make_material_resolver(filepath), filepath);
    return { std::move(assembly), std::move(realization), std::move(rows) };
}

// =============================================================================
// Private
// =============================================================================

void StateOrchestrator::set_bindings(const std::shared_ptr<State::Bindings>& bindings)
{
    m_bindings = bindings;
    if (m_bindings && !m_bindings->functions.contains("follow_buffer")) {
        m_bindings->functions.expose<&follow_buffer>();
    }
}

std::string StateOrchestrator::name_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer,
    const std::string& file, std::optional<uint64_t> row)
{
    for (const auto& [name, held] : m_bindings->buffers) {
        if (held == buffer) {
            return name;
        }
    }

    const std::string stem = file.empty() ? "buffer" : std::filesystem::path(file).stem().string();
    uint64_t number = row.value_or(0);
    std::string name = stem + "_" + std::to_string(number);
    while (m_bindings->buffers.contains(name)) {
        name = stem + "_" + std::to_string(++number);
    }

    m_bindings->buffers.emplace(name, buffer);
    if (!file.empty() && row) {
        m_bindings->assets.insert_or_assign(name, State::AssetSource { .file = file, .row = row });
    }
    return name;
}

void StateOrchestrator::wrap(const Fabric::Member& member, const BufferSlot& shown,
    const std::string& file, std::optional<uint64_t> row)
{
    auto follow = placement_from(shown.first, shown.second);
    fill(member, follow());

    Reflect::FunctionCall call;
    if (m_bindings && shown.first) {
        call = { "follow_buffer", { { "buffer", name_buffer(shown.first, file, row) } } };
        if (shown.second) {
            call.second.emplace("slot", std::to_string(*shown.second));
        }
    }
    admit(member, std::move(follow), call, row);
}

void StateOrchestrator::admit(const Fabric::Member& member, Wiring::PlacementFn follow,
    const Reflect::FunctionCall& call, std::optional<uint64_t> row)
{
    if (!m_tapestry || std::visit([](const auto& ptr) { return ptr->id() != 0; }, member)) {
        return;
    }
    if (!m_fabric) {
        m_fabric = m_tapestry->create_fabric();
    }

    auto wiring = std::visit([this](const auto& ptr) { return m_fabric->wire(ptr); }, member);
    if (follow && !as_locus(member)) {
        if (call.first.empty()) {
            wiring.place_from(std::move(follow));
        } else {
            wiring.place_from(call, std::move(follow));
        }
    }
    if (m_wiring_rule) {
        m_wiring_rule(wiring, row);
    }
    wiring.finalise();
}

Wiring::PlacementFn follow_buffer(
    const std::shared_ptr<Buffers::VKBuffer>& buffer, std::optional<uint32_t> slot)
{
    return StateOrchestrator::placement_from(buffer, slot);
}

} // namespace MayaFlux::Nexus
