#include "MeshAssemblyRealization.hpp"

#include "MayaFlux/Buffers/Network/InstanceNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Network/RecordedGeometryBuffer.hpp"
#include "MayaFlux/Nodes/Network/InstanceNetwork.hpp"

namespace MayaFlux::Kriya {

namespace {

    template <typename Frame>
    CourseSetting course_over(const Kakshya::FrameSource<Frame>& frames, CourseEdge edge)
    {
        const auto& times = frames.time_sampling();
        const size_t count = frames.count();
        const double first = times.time(0).value_or(0.0);
        const double last = times.time(count - 1).value_or(first);
        const double before_last = count > 1 ? times.time(count - 2).value_or(first) : first;

        CourseSetting setting;
        setting.in = first;
        setting.out = last > first ? last + (last - before_last) : first + 1.0;
        setting.edge = last > first ? edge : CourseEdge::Hold;
        return setting;
    }

    Kinesis::SamplerBounds bounds_of(const Kakshya::SpatialData& shot)
    {
        glm::vec3 low(std::numeric_limits<float>::max());
        glm::vec3 high(std::numeric_limits<float>::lowest());
        for (const auto& position : shot.positions) {
            low = glm::min(low, position);
            high = glm::max(high, position);
        }
        if (shot.positions.empty()) {
            return {};
        }

        const glm::vec3 pad = glm::max((high - low) * 0.05F, glm::vec3(1.0e-3F));
        return { .min = low - pad, .max = high + pad };
    }

    bool drawable(Portal::Graphics::PrimitiveTopology topology)
    {
        using Portal::Graphics::PrimitiveTopology;
        return topology == PrimitiveTopology::POINT_LIST
            || topology == PrimitiveTopology::LINE_STRIP
            || topology == PrimitiveTopology::LINE_LIST;
    }

}

std::shared_ptr<MeshAssemblyRealization> MeshAssemblyRealization::create(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, const MeshAssemblyRealizeOptions& options)
{
    if (!assembly) {
        return nullptr;
    }

    std::shared_ptr<MeshAssemblyRealization> realization(new MeshAssemblyRealization());
    realization->m_assembly = assembly;

    for (const auto& part : Kakshya::partition_assembly(*assembly, options.partition)) {
        const auto placement = options.placement ? options.placement(part) : Nodes::Network::MeshPartPlacement::Flat;

        if (part.key.instanced && (part.kind == Kakshya::MeshAssemblyRowKind::Mesh || part.kind == Kakshya::MeshAssemblyRowKind::Points)) {
            std::map<std::pair<uint64_t, std::optional<uint64_t>>, size_t> found;
            std::vector<Kakshya::MeshAssemblyGroup> groups;
            for (const uint64_t row : part.rows) {
                const auto held = assembly->placement(row);
                const std::pair<uint64_t, std::optional<uint64_t>> id { *held->source, held->material };
                auto it = found.find(id);
                if (it == found.end()) {
                    it = found.emplace(id, groups.size()).first;
                    groups.push_back({ .source = id.first, .material = id.second, .rows = {} });
                }
                groups[it->second].rows.push_back(row);
            }

            for (const auto& group : groups) {
                auto instances = Nodes::Network::make_instance_network(assembly, group);
                if (!instances) {
                    continue;
                }
                MeshAssemblyRealizedPart made;
                made.kind = part.kind;
                made.instanced = true;
                made.rows = group.rows;
                made.instance_buffer = std::make_shared<Buffers::InstanceNetworkBuffer>(instances);
                made.instances = std::move(instances);
                realization->m_parts.push_back(std::move(made));
            }
            continue;
        }

        if (part.kind == Kakshya::MeshAssemblyRowKind::Mesh) {
            auto built = Nodes::Network::make_mesh_network(assembly, part.rows, placement);
            if (!built) {
                continue;
            }
            MeshAssemblyRealizedPart made;
            made.kind = part.kind;
            made.placement = placement;
            made.rows = std::move(built->rows);
            made.network_buffer = std::make_shared<Buffers::MeshNetworkBuffer>(built->network);
            made.network = std::move(built->network);
            realization->m_parts.push_back(std::move(made));
            continue;
        }

        MeshAssemblyRealizedPart made;
        made.kind = part.kind;
        made.rows = part.rows;
        bool any = false;
        for (const uint64_t row : part.rows) {
            const auto source = assembly->placement(row)->source.value_or(0);
            const auto* spatial = assembly->spatial(source);
            const size_t capacity = std::max<size_t>(
                spatial ? spatial->positions.size() : 0, assembly->spatial_max_vertices(source).value_or(0));
            if (!spatial || capacity == 0 || !drawable(spatial->topology)) {
                made.spatial_buffers.push_back(nullptr);
                continue;
            }
            made.spatial_buffers.push_back(std::make_shared<Buffers::RecordedGeometryBuffer>(
                static_cast<uint32_t>(capacity), spatial->topology,
                options.bounds.value_or(bounds_of(*spatial)),
                options.spatial_field ? options.spatial_field(part, row) : nullptr));
            any = true;
        }
        if (any) {
            realization->m_parts.push_back(std::move(made));
        }
    }

    if (realization->m_parts.empty()) {
        return nullptr;
    }
    return realization;
}

MotionReflow::Sink MeshAssemblyRealization::placement_sink(size_t part) const
{
    if (part >= m_parts.size()) {
        return {};
    }

    const auto& held = m_parts[part];
    if (held.network) {
        return slot_sink(held.network, held.rows, held.placement);
    }
    if (held.instances) {
        return instance_sink(held.instances, held.rows);
    }

    std::vector<MotionReflow::Sink> sinks;
    for (size_t i = 0; i < held.spatial_buffers.size(); ++i) {
        if (held.spatial_buffers[i]) {
            sinks.push_back(Kriya::placement_sink(held.spatial_buffers[i], held.rows[i]));
        }
    }
    return fan_out<Kakshya::MeshAssemblyLocals>(std::move(sinks));
}

MotionReflow::Sink MeshAssemblyRealization::placement_sink() const
{
    std::vector<MotionReflow::Sink> sinks;
    sinks.reserve(m_parts.size());
    for (size_t i = 0; i < m_parts.size(); ++i) {
        sinks.push_back(placement_sink(i));
    }
    return fan_out<Kakshya::MeshAssemblyLocals>(std::move(sinks));
}

std::vector<CourseRoute<Kakshya::MeshData>> MeshAssemblyRealization::mesh_routes(CourseEdge edge) const
{
    std::map<size_t, std::vector<MeshReflow::Sink>> sinks;

    for (const auto& part : m_parts) {
        if (part.kind != Kakshya::MeshAssemblyRowKind::Mesh) {
            continue;
        }

        if (part.instances) {
            const size_t source = m_assembly->placement(part.rows.front())->source.value_or(0);
            if (m_assembly->mesh_frames(source)) {
                sinks[source].push_back(instance_geometry_sink(part.instances));
            }
            continue;
        }

        for (size_t slot = 0; slot < part.rows.size(); ++slot) {
            const auto held = m_assembly->placement(part.rows[slot]);
            if (!held->source || !part.network->get_slot(static_cast<uint32_t>(slot)).node
                || !m_assembly->mesh_frames(*held->source)) {
                continue;
            }
            sinks[*held->source].push_back(slot_geometry_sink(part.network, static_cast<uint32_t>(slot)));
        }
    }

    std::vector<CourseRoute<Kakshya::MeshData>> routes;
    for (auto& [source, list] : sinks) {
        const auto frames = m_assembly->mesh_frames(source);
        CourseRoute<Kakshya::MeshData> route;
        route.course = course_over(*frames, edge);
        route.source = frames;
        route.sink = fan_out<Kakshya::MeshData>(std::move(list));
        routes.push_back(std::move(route));
    }
    return routes;
}

std::vector<CourseRoute<Kakshya::SpatialData>> MeshAssemblyRealization::spatial_routes(CourseEdge edge) const
{
    std::map<size_t, std::vector<SpatialReflow::Sink>> sinks;

    for (const auto& part : m_parts) {
        for (size_t i = 0; i < part.spatial_buffers.size(); ++i) {
            if (!part.spatial_buffers[i]) {
                continue;
            }
            const size_t source = m_assembly->placement(part.rows[i])->source.value_or(0);
            if (m_assembly->spatial_frames(source)) {
                sinks[source].push_back(spatial_sink(part.spatial_buffers[i]));
            }
        }
    }

    std::vector<CourseRoute<Kakshya::SpatialData>> routes;
    for (auto& [source, list] : sinks) {
        const auto frames = m_assembly->spatial_frames(source);
        CourseRoute<Kakshya::SpatialData> route;
        route.course = course_over(*frames, edge);
        route.source = frames;
        route.sink = fan_out<Kakshya::SpatialData>(std::move(list));
        routes.push_back(std::move(route));
    }
    return routes;
}

size_t MeshAssemblyRealization::bind_materials(const MeshAssemblyMaterialResolver& resolver) const
{
    if (!resolver) {
        return 0;
    }

    const auto& materials = m_assembly->get_data().materials;
    std::map<uint64_t, std::shared_ptr<Core::VKImage>> images;
    const auto image_of = [&](uint64_t material) -> std::shared_ptr<Core::VKImage> {
        if (material >= materials.size()) {
            return nullptr;
        }
        const auto it = images.find(material);
        if (it != images.end()) {
            return it->second;
        }
        return images.emplace(material, resolver(materials[material])).first->second;
    };

    size_t bound = 0;
    for (const auto& part : m_parts) {
        if (part.network) {
            for (size_t slot = 0; slot < part.rows.size(); ++slot) {
                const auto held = m_assembly->placement(part.rows[slot]);
                if (!held->material || !part.network->get_slot(static_cast<uint32_t>(slot)).node) {
                    continue;
                }
                if (auto image = image_of(*held->material)) {
                    part.network->set_slot_texture(static_cast<uint32_t>(slot), std::move(image));
                    ++bound;
                }
            }
        } else if (part.instances && part.instance_buffer) {
            const auto held = m_assembly->placement(part.rows.front());
            if (!held->material) {
                continue;
            }
            if (auto image = image_of(*held->material)) {
                part.instance_buffer->bind_instance_textures({ std::move(image) });
                ++bound;
            }
        }
    }
    return bound;
}

size_t MeshAssemblyRealization::apply_surfaces() const
{
    const auto& materials = m_assembly->get_data().materials;
    size_t tinted = 0;
    for (const auto& part : m_parts) {
        if (!part.instances) {
            continue;
        }
        for (size_t slot = 0; slot < part.rows.size(); ++slot) {
            const auto held = m_assembly->placement(part.rows[slot]);
            if (!held->material || *held->material >= materials.size()) {
                continue;
            }
            const auto* surface = materials[*held->material].facets.get<Kakshya::MeshAssemblySurface>();
            if (!surface) {
                continue;
            }
            auto& target = part.instances->get_slot(static_cast<uint32_t>(slot));
            target.tint = glm::vec3(surface->base_color);
            target.dirty = true;
            ++tinted;
        }
    }
    return tinted;
}

void MeshAssemblyRealization::show_rest() const
{
    std::map<size_t, std::shared_ptr<const Kakshya::SpatialData>> shots;

    for (const auto& part : m_parts) {
        for (size_t i = 0; i < part.spatial_buffers.size(); ++i) {
            if (!part.spatial_buffers[i]) {
                continue;
            }
            const size_t source = m_assembly->placement(part.rows[i])->source.value_or(0);
            auto& shot = shots[source];
            if (!shot) {
                shot = std::make_shared<const Kakshya::SpatialData>(*m_assembly->spatial(source));
            }
            part.spatial_buffers[i]->write_frame(shot);
        }
    }
}

} // namespace MayaFlux::Kriya
