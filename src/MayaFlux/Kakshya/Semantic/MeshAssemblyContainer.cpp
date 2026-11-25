#include "MeshAssemblyContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/CompositeDescribe.hpp"
#include "MayaFlux/Kakshya/Region/RegionSegment.hpp"
#include "MayaFlux/Kinesis/MatrixTransforms.hpp"

namespace MayaFlux::Kakshya {

namespace detail {

    void renumber_materials(Facets& facets, std::span<const uint64_t> materials)
    {
        const auto* held = facets.get<MeshAssemblyFaceSets>();
        if (!held) {
            return;
        }
        MeshAssemblyFaceSets face_sets = *held;
        for (auto& set : face_sets.sets) {
            if (!set.material) {
                continue;
            }
            set.material = *set.material < materials.size()
                ? std::optional<uint64_t> { materials[*set.material] }
                : std::nullopt;
        }
        facets.set(std::move(face_sets));
    }

    MeshAssemblyVisibility renumber_rows(
        const MeshAssemblyVisibility& held, std::span<const std::optional<uint64_t>> rows)
    {
        MeshAssemblyVisibility out;
        for (const auto& entry : held.entries) {
            if (entry.row < rows.size() && rows[entry.row]) {
                out.entries.push_back(entry);
                out.entries.back().row = *rows[entry.row];
            }
        }
        return out;
    }

    MeshAssemblyMetadata renumber_rows(
        const MeshAssemblyMetadata& held, std::span<const std::optional<uint64_t>> rows, bool keep_scene)
    {
        MeshAssemblyMetadata out;
        for (const auto& entry : held.entries) {
            if (!entry.row) {
                if (keep_scene) {
                    out.entries.push_back(entry);
                }
            } else if (*entry.row < rows.size() && rows[*entry.row]) {
                out.entries.push_back(entry);
                out.entries.back().row = rows[*entry.row];
            }
        }
        return out;
    }

}

namespace {

    ContainerDataStructure assembly_structure()
    {
        return { DataModality::TENSOR_ND, OrganizationStrategy::INTERLEAVED };
    }

    std::optional<MeshAssemblyPlacement> read_placement(const CompositeArray& rows, size_t row)
    {
        const auto element = rows.at(row);
        if (!element) {
            return std::nullopt;
        }
        MeshAssemblyPlacement placement;
        std::string error;
        if (!overlay_described(*element, placement, error)) {
            return std::nullopt;
        }
        return placement;
    }

    std::optional<MeshAssemblyData> extract_rows(const MeshAssemblyData& data, size_t start, size_t count)
    {
        std::vector<MeshAssemblyPlacement> rows;
        rows.reserve(count);
        std::vector<bool> source_used(data.sources.size(), false);
        std::vector<bool> material_used(data.materials.size(), false);
        std::vector<bool> skeleton_used(data.skeletons.size(), false);

        std::vector<std::optional<uint64_t>> row_map(data.placements.size());
        for (size_t row = start; row < start + count; ++row) {
            row_map[row] = row - start;
        }

        size_t unbound = 0;
        for (size_t row = start; row < start + count; ++row) {
            auto placement = read_placement(data.placements, row);
            if (!placement) {
                return std::nullopt;
            }
            if (placement->source) {
                source_used[*placement->source] = true;
            }
            if (placement->material) {
                material_used[*placement->material] = true;
            }
            if (placement->skeleton) {
                const auto& joints = data.skeletons[*placement->skeleton];
                if (std::ranges::all_of(joints, [&](uint64_t joint) { return row_map[joint].has_value(); })) {
                    skeleton_used[*placement->skeleton] = true;
                } else {
                    placement->skeleton.reset();
                    ++unbound;
                }
            }
            rows.push_back(std::move(*placement));
        }
        if (unbound > 0) {
            MF_WARN(Journal::Component::Kakshya, Journal::Context::Configuration,
                "MeshAssemblyContainer: {} copied rows have joints outside the region, so they are copied without a skeleton",
                unbound);
        }

        for (size_t i = 0; i < data.sources.size(); ++i) {
            const auto* face_sets = source_used[i] ? data.sources[i].facets.get<MeshAssemblyFaceSets>() : nullptr;
            if (!face_sets) {
                continue;
            }
            for (const auto& set : face_sets->sets) {
                if (set.material && *set.material < data.materials.size()) {
                    material_used[*set.material] = true;
                }
            }
        }

        MeshAssemblyData out;
        out.facets = data.facets;
        if (const auto* held = data.facets.get<MeshAssemblyVisibility>()) {
            out.facets.set(detail::renumber_rows(*held, row_map));
        }
        if (const auto* held = data.facets.get<MeshAssemblyMetadata>()) {
            out.facets.set(detail::renumber_rows(*held, row_map, true));
        }

        std::vector<uint64_t> source_map(data.sources.size());
        std::vector<uint64_t> material_map(data.materials.size());
        std::vector<uint64_t> skeleton_map(data.skeletons.size());
        for (size_t i = 0; i < data.materials.size(); ++i) {
            if (material_used[i]) {
                material_map[i] = out.materials.size();
                out.materials.push_back(data.materials[i]);
            }
        }
        for (size_t i = 0; i < data.sources.size(); ++i) {
            if (source_used[i]) {
                source_map[i] = out.sources.size();
                out.sources.push_back(data.sources[i]);
                detail::renumber_materials(out.sources.back().facets, material_map);
            }
        }
        for (size_t i = 0; i < data.skeletons.size(); ++i) {
            if (!skeleton_used[i]) {
                continue;
            }
            skeleton_map[i] = out.skeletons.size();
            auto skeleton = data.skeletons[i];
            for (auto& joint : skeleton) {
                joint = *row_map[joint];
            }
            out.skeletons.push_back(std::move(skeleton));
        }

        CompositeInsertion insertion(out.placements);
        std::string error;
        for (auto& placement : rows) {
            if (placement.parent) {
                placement.parent = *placement.parent >= start ? std::optional<uint64_t> { *placement.parent - start } : std::nullopt;
            }
            if (placement.source) {
                placement.source = source_map[*placement.source];
            }
            if (placement.material) {
                placement.material = material_map[*placement.material];
            }
            if (placement.skeleton) {
                placement.skeleton = skeleton_map[*placement.skeleton];
            }
            if (!write_described(insertion, insertion.append(), placement, error)) {
                return std::nullopt;
            }
        }

        for (const auto& motion : data.motions) {
            MeshAssemblyMotion kept { .name = motion.name, .channels = {} };
            for (const auto& channel : motion.channels) {
                if (channel.row >= start && channel.row < start + count) {
                    kept.channels.push_back(channel);
                    kept.channels.back().row -= start;
                }
            }
            for (const auto& weights : motion.weights) {
                if (weights.row >= start && weights.row < start + count) {
                    kept.weights.push_back(weights);
                    kept.weights.back().row -= start;
                }
            }
            if (!kept.channels.empty() || !kept.weights.empty()) {
                out.motions.push_back(std::move(kept));
            }
        }
        return out;
    }

    bool channels_are_valid(const MeshAssemblyMotion& motion, size_t rows, std::string& error)
    {
        std::vector<bool> targeted(rows, false);
        for (const auto& channel : motion.channels) {
            if (channel.row >= rows) {
                error = "motion " + motion.name + " targets a row that does not exist";
                return false;
            }
            if (targeted[channel.row]) {
                error = "motion " + motion.name + " targets a row twice";
                return false;
            }
            targeted[channel.row] = true;
        }
        std::vector<bool> weighted(rows, false);
        for (const auto& weights : motion.weights) {
            if (weights.row >= rows || weighted[weights.row]) {
                error = "motion " + motion.name + " weights a row that does not exist, or one row twice";
                return false;
            }
            weighted[weights.row] = true;
        }
        return true;
    }

}

MeshAssemblyContainer::MeshAssemblyContainer()
    : SemanticContainer<MeshAssemblyData>(assembly_structure())
{
}

MeshAssemblyContainer::MeshAssemblyContainer(MeshAssemblyData data)
    : SemanticContainer<MeshAssemblyData>(assembly_structure())
    , m_data(std::move(data))
{
    std::string reason;
    if (!validate(m_data, reason)) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya, Journal::Context::Configuration,
            std::source_location::current(),
            "MeshAssemblyContainer: " + reason);
    }
}

MeshAssemblyContainer::~MeshAssemblyContainer() = default;

bool MeshAssemblyContainer::validate(const MeshAssemblyData& data, std::string& error)
{
    if (!data.placements.layout().same_structure(MeshAssemblyData::placement_layout())) {
        error = "the placement table does not have the placement layout";
        return false;
    }

    const size_t rows = data.placements.size();
    for (size_t row = 0; row < rows; ++row) {
        const auto placement = read_placement(data.placements, row);
        if (!placement) {
            error = "a placement row is unreadable";
            return false;
        }
        if (placement->parent && *placement->parent >= row) {
            error = "a parent does not precede its child";
            return false;
        }
        if (placement->source && *placement->source >= data.sources.size()) {
            error = "a source index is out of range";
            return false;
        }
        if (placement->material && *placement->material >= data.materials.size()) {
            error = "a material index is out of range";
            return false;
        }
        if (placement->skeleton && *placement->skeleton >= data.skeletons.size()) {
            error = "a skeleton index is out of range";
            return false;
        }
    }

    for (const auto& skeleton : data.skeletons) {
        if (skeleton.empty()
            || std::ranges::any_of(skeleton, [rows](uint64_t joint) { return joint >= rows; })) {
            error = "a skeleton has no joints or names a row that does not exist";
            return false;
        }
    }

    return std::ranges::all_of(data.motions, [&](const MeshAssemblyMotion& motion) {
        return channels_are_valid(motion, rows, error);
    });
}

size_t MeshAssemblyContainer::add_source(MeshAssemblySource source)
{
    m_data.sources.push_back(std::move(source));
    return m_data.sources.size() - 1;
}

bool MeshAssemblyContainer::set_mesh_frames(size_t source, std::shared_ptr<const FrameSource<MeshData>> frames)
{
    if (mesh(source) == nullptr) {
        return false;
    }
    m_mesh_frames.resize(std::max(m_mesh_frames.size(), source + 1));
    m_mesh_frames[source] = std::move(frames);
    return true;
}

bool MeshAssemblyContainer::set_spatial_frames(size_t source, std::shared_ptr<const FrameSource<SpatialData>> frames,
    std::optional<uint32_t> max_vertices)
{
    if (spatial(source) == nullptr) {
        return false;
    }
    m_spatial_frames.resize(std::max(m_spatial_frames.size(), source + 1));
    m_spatial_max.resize(m_spatial_frames.size());
    m_spatial_frames[source] = std::move(frames);
    m_spatial_max[source] = max_vertices;
    return true;
}

bool MeshAssemblyContainer::set_lens_frames(size_t source, std::shared_ptr<const FrameSource<Kinesis::Lens>> frames)
{
    if (lens(source) == nullptr) {
        return false;
    }
    m_lens_frames.resize(std::max(m_lens_frames.size(), source + 1));
    m_lens_frames[source] = std::move(frames);
    return true;
}

const MeshAssemblyLight* MeshAssemblyContainer::light(size_t source) const noexcept
{
    return source < m_data.sources.size() ? std::get_if<MeshAssemblyLight>(&m_data.sources[source].geometry) : nullptr;
}

const Kinesis::Lens* MeshAssemblyContainer::lens(size_t source) const noexcept
{
    return source < m_data.sources.size() ? std::get_if<Kinesis::Lens>(&m_data.sources[source].geometry) : nullptr;
}

std::shared_ptr<const FrameSource<Kinesis::Lens>> MeshAssemblyContainer::lens_frames(size_t source) const
{
    return source < m_lens_frames.size() ? m_lens_frames[source] : nullptr;
}

std::shared_ptr<const FrameSource<MeshData>> MeshAssemblyContainer::mesh_frames(size_t source) const
{
    return source < m_mesh_frames.size() ? m_mesh_frames[source] : nullptr;
}

std::shared_ptr<const FrameSource<SpatialData>> MeshAssemblyContainer::spatial_frames(size_t source) const
{
    return source < m_spatial_frames.size() ? m_spatial_frames[source] : nullptr;
}

std::optional<uint32_t> MeshAssemblyContainer::spatial_max_vertices(size_t source) const
{
    return source < m_spatial_max.size() ? m_spatial_max[source] : std::nullopt;
}

size_t MeshAssemblyContainer::add_material(MeshAssemblyMaterial material)
{
    m_data.materials.push_back(std::move(material));
    return m_data.materials.size() - 1;
}

std::optional<size_t> MeshAssemblyContainer::add_skeleton(std::vector<uint64_t> joints)
{
    const size_t rows = m_data.placements.size();
    if (joints.empty()
        || std::ranges::any_of(joints, [rows](uint64_t joint) { return joint >= rows; })) {
        return std::nullopt;
    }
    m_data.skeletons.push_back(std::move(joints));
    return m_data.skeletons.size() - 1;
}

bool MeshAssemblyContainer::set_row_skeleton(size_t row, std::optional<uint64_t> skeleton)
{
    if (skeleton && *skeleton >= m_data.skeletons.size()) {
        return false;
    }
    auto held = read_placement(m_data.placements, row);
    if (!held) {
        return false;
    }
    CompositeInsertion insertion(m_data.placements);
    held->skeleton = skeleton;
    std::string reason;
    return write_described(insertion, row, *held, reason);
}

const std::vector<uint64_t>* MeshAssemblyContainer::skeleton(size_t index) const noexcept
{
    return index < m_data.skeletons.size() ? &m_data.skeletons[index] : nullptr;
}

std::optional<size_t> MeshAssemblyContainer::add_placement(const MeshAssemblyPlacement& placement)
{
    if ((placement.parent && *placement.parent >= m_data.placements.size())
        || (placement.source && *placement.source >= m_data.sources.size())
        || (placement.material && *placement.material >= m_data.materials.size())
        || (placement.skeleton && *placement.skeleton >= m_data.skeletons.size())) {
        return std::nullopt;
    }

    CompositeInsertion insertion(m_data.placements);
    const size_t row = insertion.append();
    std::string reason;
    if (!write_described(insertion, row, placement, reason)) {
        return std::nullopt;
    }
    return row;
}

std::optional<MeshAssemblyPlacement> MeshAssemblyContainer::placement(size_t row) const
{
    return read_placement(m_data.placements, row);
}

std::optional<size_t> MeshAssemblyContainer::add_motion(MeshAssemblyMotion motion)
{
    std::string reason;
    if (!channels_are_valid(motion, m_data.placements.size(), reason)) {
        return std::nullopt;
    }
    m_data.motions.push_back(std::move(motion));
    return m_data.motions.size() - 1;
}

const MeshData* MeshAssemblyContainer::mesh(size_t source) const noexcept
{
    return source < m_data.sources.size() ? std::get_if<MeshData>(&m_data.sources[source].geometry) : nullptr;
}

bool MeshAssemblyContainer::set_source_facets(size_t source, Facets facets)
{
    if (source >= m_data.sources.size()) {
        return false;
    }
    m_data.sources[source].facets = std::move(facets);
    return true;
}

std::shared_ptr<const MeshSkin> MeshAssemblyContainer::skin(size_t source) const
{
    if (source >= m_data.sources.size()) {
        return nullptr;
    }
    const auto* held = m_data.sources[source].facets.get<std::shared_ptr<const MeshSkin>>();
    return held ? *held : nullptr;
}

const SpatialData* MeshAssemblyContainer::spatial(size_t source) const noexcept
{
    return source < m_data.sources.size() ? std::get_if<SpatialData>(&m_data.sources[source].geometry) : nullptr;
}

ForestIndex MeshAssemblyContainer::index_forest() const
{
    std::string reason;
    return index_composite_forest(m_data.placements, MeshAssemblyData::parent_column(), reason).value();
}

std::vector<glm::mat4> MeshAssemblyContainer::local_transforms() const
{
    std::vector<glm::mat4> locals;
    locals.reserve(m_data.placements.size());
    for (size_t row = 0; row < m_data.placements.size(); ++row) {
        locals.push_back(read_placement(m_data.placements, row)->local);
    }
    return locals;
}

std::vector<glm::mat4> MeshAssemblyContainer::world_transforms() const
{
    return world_transforms(local_transforms());
}

std::vector<glm::mat4> MeshAssemblyContainer::world_transforms(std::span<const glm::mat4> locals) const
{
    if (locals.size() != m_data.placements.size()) {
        return {};
    }

    std::vector<glm::mat4> world;
    world.reserve(locals.size());
    for (size_t row = 0; row < locals.size(); ++row) {
        const auto parent = read_placement(m_data.placements, row)->parent;
        world.push_back(parent ? world[*parent] * locals[row] : locals[row]);
    }
    return world;
}

std::vector<MeshAssemblyGroup> MeshAssemblyContainer::groups() const
{
    constexpr uint64_t no_material = std::numeric_limits<uint64_t>::max();

    std::vector<MeshAssemblyGroup> result;
    std::map<std::pair<uint64_t, uint64_t>, size_t> position;
    for (size_t row = 0; row < m_data.placements.size(); ++row) {
        const auto placement = read_placement(m_data.placements, row);
        if (!placement->source) {
            continue;
        }
        const auto [it, inserted] = position.try_emplace(
            std::pair<uint64_t, uint64_t> { *placement->source, placement->material.value_or(no_material) },
            result.size());
        if (inserted) {
            result.push_back({ .source = *placement->source, .material = placement->material, .rows = {} });
        }
        result[it->second].rows.push_back(row);
    }
    return result;
}

std::optional<std::vector<glm::mat4>> MeshAssemblyContainer::sample(
    size_t motion, double time, Kinesis::OutOfRange policy) const
{
    if (motion >= m_data.motions.size()) {
        return std::nullopt;
    }

    auto locals = local_transforms();
    for (const auto& channel : m_data.motions[motion].channels) {
        glm::mat4& local = locals[channel.row];
        const auto rest = Kinesis::decompose_transform(local).value_or(
            std::tuple { glm::vec3(0.0F), glm::quat(1.0F, 0.0F, 0.0F, 0.0F), glm::vec3(1.0F) });
        auto [translation, rotation, scale] = rest;

        if (channel.translation) {
            if (const auto value = channel.translation->sample(time, policy)) {
                translation = *value;
            }
        }
        if (channel.rotation) {
            if (const auto value = channel.rotation->sample(time, policy)) {
                rotation = glm::quat(value->w, value->x, value->y, value->z);
            }
        }
        if (channel.scale) {
            if (const auto value = channel.scale->sample(time, policy)) {
                scale = *value;
            }
        }
        local = Kinesis::compose_transform(translation, rotation, scale);
    }
    return locals;
}

std::vector<DataDimension> MeshAssemblyContainer::get_dimensions() const
{
    return { DataDimension("placements", m_data.placements.size(), 1, DataDimension::Role::CUSTOM) };
}

std::vector<MeshAssemblyData> MeshAssemblyContainer::get_region_data(const Region& region) const
{
    const auto selected = m_data.placements.slice(region);
    if (!selected) {
        return {};
    }

    auto rows = extract_rows(m_data, selected->start(), selected->size());
    if (!rows) {
        return {};
    }

    std::vector<MeshAssemblyData> result;
    result.push_back(std::move(*rows));
    return result;
}

std::vector<MeshAssemblyData> MeshAssemblyContainer::get_segments_data(
    const std::vector<RegionSegment>& segments) const
{
    std::vector<MeshAssemblyData> result;
    result.reserve(segments.size());

    for (const auto& segment : segments) {
        const auto& region = segment.source_region;

        if (region.start_coordinates.size() != 1 || region.end_coordinates.size() != 1
            || segment.offset_in_region.size() != 1 || segment.segment_size.size() != 1) {
            continue;
        }

        const uint64_t source_start = region.start_coordinates[0];
        const uint64_t source_end = region.end_coordinates[0];
        const uint64_t offset = segment.offset_in_region[0];
        const uint64_t count = segment.segment_size[0];

        if (source_start > source_end || offset > source_end - source_start || count == 0) {
            continue;
        }

        const uint64_t start = source_start + offset;
        if (count - 1 > source_end - start) {
            continue;
        }

        auto copied = get_region_data(Region(
            std::vector<uint64_t> { start }, std::vector<uint64_t> { start + count - 1 }));
        if (!copied.empty()) {
            result.push_back(std::move(copied.front()));
        }
    }
    return result;
}

bool MeshAssemblyContainer::is_region_loaded(const Region& region) const
{
    return m_data.placements.slice(region).has_value();
}

uint64_t MeshAssemblyContainer::coordinates_to_linear_index(const std::vector<uint64_t>& coordinates) const
{
    return coordinates.size() == 1 ? coordinates[0] : 0;
}

std::vector<uint64_t> MeshAssemblyContainer::linear_index_to_coordinates(uint64_t linear_index) const
{
    return { linear_index };
}

void MeshAssemblyContainer::clear()
{
    m_data = MeshAssemblyData {};
    m_mesh_frames.clear();
    m_spatial_frames.clear();
    m_lens_frames.clear();
    m_spatial_max.clear();
    clear_region_groups();
}

const void* MeshAssemblyContainer::get_raw_data() const
{
    const auto& bytes = std::get<std::vector<uint8_t>>(m_data.placements.element_data());
    return bytes.empty() ? nullptr : bytes.data();
}

}
