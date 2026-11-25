#include "AlembicModelWriter.hpp"

#include "MayaFlux/IO/Spatial/AlembicShared.hpp"

#include "MayaFlux/IO/FileWriter.hpp"
#include "MayaFlux/IO/Image/STBImageWriter.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kakshya/Semantic/MeshAssemblyContainer.hpp"
#include "MayaFlux/Kinesis/MatrixTransforms.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>

namespace MayaFlux::IO {

namespace {

    using namespace Alembic::AbcGeom;
    using namespace Alembic::AbcMaterial;
    using alembic::GeomParamVariant;
    using alembic::set_attribute;
    using Portal::Graphics::PrimitiveTopology;
    using TimesPtr = Alembic::AbcCoreAbstract::TimeSamplingPtr;
    using Metadata = std::vector<const Kakshya::MeshAssemblyMetadata::Entry*>;

    TimesPtr acyclic(const std::vector<double>& times)
    {
        if (times.size() < 2 || std::ranges::adjacent_find(times, std::greater_equal<> {}) != times.end()) {
            return nullptr;
        }
        return TimesPtr(new Alembic::AbcCoreAbstract::TimeSampling(
            Alembic::AbcCoreAbstract::TimeSamplingType(Alembic::AbcCoreAbstract::TimeSamplingType::kAcyclic),
            times));
    }

    TimesPtr times_of(const Kinesis::TimeSampling& sampling, size_t count)
    {
        std::vector<double> times;
        times.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            if (const auto time = sampling.time(i)) {
                times.push_back(*time);
            }
        }
        return times.size() == count ? acyclic(times) : nullptr;
    }

    V3f to_v3f(const glm::vec3& v) { return { v.x, v.y, v.z }; }

    M44d to_m44d(const glm::mat4& m)
    {
        M44d out;
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                out[column][row] = m[column][row];
            }
        }
        return out;
    }

    bool is_identity(const glm::mat4& m)
    {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                if (std::abs(m[column][row] - (column == row ? 1.0F : 0.0F)) > 1.0e-7F) {
                    return false;
                }
            }
        }
        return true;
    }

    template <class T>
    const T* data_as(const auto& values)
    {
        return reinterpret_cast<const T*>(values.data());
    }

    /** @brief Texture coordinates are held with v downwards and Alembic's run upwards. */
    std::vector<glm::vec2> flipped(std::vector<glm::vec2> uvs)
    {
        for (auto& uv : uvs) {
            uv.y = 1.0F - uv.y;
        }
        return uvs;
    }

    /** @brief Names unique among the children of each object. */
    class Names {
    public:
        std::string take(const std::string& scope, std::string wanted, const std::string& fallback)
        {
            if (wanted.empty()) {
                wanted = fallback;
            }
            std::ranges::replace(wanted, '/', '_');
            auto& used = m_used[scope];
            std::string name = wanted;
            for (size_t n = 2; !used.insert(name).second; ++n) {
                name = wanted + "_" + std::to_string(n);
            }
            return name;
        }

        std::string child_of(OObject& parent, const std::string& wanted, const std::string& fallback)
        {
            return take(parent.getFullName(), wanted, fallback);
        }

    private:
        std::unordered_map<std::string, std::unordered_set<std::string>> m_used;
    };

    void write_user_values(const OCompoundProperty& top, const Metadata& entries)
    {
        std::map<std::string, OCompoundProperty> groups;
        std::unordered_set<std::string> used;

        for (const auto* entry : entries) {
            OCompoundProperty compound = top;
            std::string path;
            std::string leaf = entry->key;
            for (size_t slash = leaf.find('/'); slash != std::string::npos; slash = leaf.find('/')) {
                const std::string segment = leaf.substr(0, slash);
                leaf.erase(0, slash + 1);
                path += segment + "/";
                auto found = groups.find(path);
                if (found == groups.end()) {
                    if (!used.insert(path.substr(0, path.size() - 1)).second) {
                        break;
                    }
                    found = groups.emplace(path, OCompoundProperty(compound, segment)).first;
                }
                compound = found->second;
            }
            if (leaf.empty() || leaf.find('/') != std::string::npos || !used.insert(path + leaf).second) {
                continue;
            }

            std::visit(
                [&](const auto& value) {
                    using Value = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::same_as<Value, bool>) {
                        OBoolProperty(compound, leaf).set(Alembic::Util::bool_t(value));
                    } else if constexpr (std::same_as<Value, int64_t>) {
                        OInt64Property(compound, leaf).set(value);
                    } else if constexpr (std::same_as<Value, uint64_t>) {
                        OUInt64Property(compound, leaf).set(value);
                    } else if constexpr (std::same_as<Value, double>) {
                        ODoubleProperty(compound, leaf).set(value);
                    } else if constexpr (std::same_as<Value, glm::vec3>) {
                        OV3fProperty(compound, leaf).set(to_v3f(value));
                    } else {
                        OStringProperty(compound, leaf).set(value);
                    }
                },
                entry->value);
        }
    }

    void write_visibility(OObject& object, const Kakshya::MeshAssemblyVisibility::Entry& entry)
    {
        if (entry.keys) {
            std::vector<double> times;
            for (size_t k = 0; k < entry.keys->size(); ++k) {
                times.push_back(entry.keys->sampling().time(k).value_or(0.0));
            }
            if (const auto sampling = acyclic(times)) {
                auto property = CreateVisibilityProperty(object, sampling);
                for (const float shown : entry.keys->values()) {
                    property.set(shown > 0.5F ? kVisibilityVisible : kVisibilityHidden);
                }
                return;
            }
        }
        CreateVisibilityProperty(object, 0U).set(entry.visible ? kVisibilityVisible : kVisibilityHidden);
    }

    /**
     * @brief A camera sample whose screen window is the lens's, found through the overscan
     *        of each edge, which the window follows linearly.
     */
    CameraSample camera_sample(const Kinesis::Lens& lens)
    {
        CameraSample out;
        out.setFocalLength(lens.focal_length);
        out.setHorizontalAperture(lens.horizontal_aperture);
        out.setVerticalAperture(lens.vertical_aperture);
        out.setNearClippingPlane(lens.near);
        out.setFarClippingPlane(lens.far);
        out.setFStop(lens.f_stop);
        out.setFocusDistance(lens.focus_distance);

        double top {};
        double bottom {};
        double left {};
        double right {};
        out.getScreenWindow(top, bottom, left, right);

        CameraSample probe = out;
        probe.setOverScanTop(1.0);
        probe.setOverScanBottom(1.0);
        probe.setOverScanLeft(1.0);
        probe.setOverScanRight(1.0);
        double probe_top {};
        double probe_bottom {};
        double probe_left {};
        double probe_right {};
        probe.getScreenWindow(probe_top, probe_bottom, probe_left, probe_right);

        const auto solve = [](double target, double base, double probed) {
            return probed != base ? (target - base) / (probed - base) : 0.0;
        };
        out.setOverScanTop(solve(lens.window.top, top, probe_top));
        out.setOverScanBottom(solve(lens.window.bottom, bottom, probe_bottom));
        out.setOverScanLeft(solve(lens.window.left, left, probe_left));
        out.setOverScanRight(solve(lens.window.right, right, probe_right));
        return out;
    }

    /** @brief The vertices of a mesh sample arranged as Alembic's points, faces and corners. */
    struct Cage {
        std::vector<Kakshya::MeshVertex> vertices;
        std::vector<uint32_t> point_of;
        std::vector<uint32_t> corner_vertex;
        std::vector<int32_t> indices;
        std::vector<int32_t> counts;
        std::vector<glm::vec3> positions;

        template <class T>
        std::pair<std::vector<T>, GeometryScope> encode(const std::vector<T>& per_vertex, bool compact) const
        {
            if (compact) {
                std::vector<T> per_point(positions.size(), T {});
                std::vector<bool> seen(positions.size(), false);
                bool agree = true;
                for (size_t v = 0; v < per_vertex.size() && agree; ++v) {
                    const uint32_t point = point_of[v];
                    if (!seen[point]) {
                        per_point[point] = per_vertex[v];
                        seen[point] = true;
                    } else {
                        agree = per_point[point] == per_vertex[v];
                    }
                }
                if (agree) {
                    return { std::move(per_point), kVertexScope };
                }
            }

            std::vector<T> per_corner;
            per_corner.reserve(corner_vertex.size());
            for (const uint32_t v : corner_vertex) {
                per_corner.push_back(per_vertex[v]);
            }
            return { std::move(per_corner), kFacevaryingScope };
        }

        template <class T, class Member>
        [[nodiscard]] std::vector<T> column(Member member) const
        {
            std::vector<T> out;
            out.reserve(vertices.size());
            for (const auto& vertex : vertices) {
                out.push_back(vertex.*member);
            }
            return out;
        }
    };

    std::optional<Cage> cage_of(const Kakshya::MeshData& mesh, const Kakshya::MeshAssemblyPointMap* map)
    {
        const auto* bytes = std::get_if<std::vector<uint8_t>>(&mesh.vertex_variant);
        const auto* triangles = std::get_if<std::vector<uint32_t>>(&mesh.index_variant);
        if (!bytes || !triangles || mesh.layout.stride_bytes != sizeof(Kakshya::MeshVertex)) {
            return std::nullopt;
        }

        Cage cage;
        cage.vertices.resize(bytes->size() / sizeof(Kakshya::MeshVertex));
        std::memcpy(cage.vertices.data(), bytes->data(), cage.vertices.size() * sizeof(Kakshya::MeshVertex));

        const bool mapped = map && map->of_vertex.size() == cage.vertices.size()
            && std::ranges::all_of(map->of_vertex, [map](uint32_t p) { return p < map->point_count; });
        if (mapped) {
            cage.point_of = map->of_vertex;
            cage.positions.assign(map->point_count, glm::vec3(0.0F));
        } else {
            cage.point_of.resize(cage.vertices.size());
            std::iota(cage.point_of.begin(), cage.point_of.end(), 0U);
            cage.positions.assign(cage.vertices.size(), glm::vec3(0.0F));
        }
        for (size_t v = 0; v < cage.vertices.size(); ++v) {
            cage.positions[cage.point_of[v]] = cage.vertices[v].position;
        }

        const auto add_face = [&cage](std::span<const uint32_t> corners) {
            for (size_t k = corners.size(); k > 0; --k) {
                const uint32_t v = corners[k - 1];
                if (v >= cage.vertices.size()) {
                    return false;
                }
                cage.corner_vertex.push_back(v);
                cage.indices.push_back(static_cast<int32_t>(cage.point_of[v]));
            }
            cage.counts.push_back(static_cast<int32_t>(corners.size()));
            return true;
        };

        if (mesh.faces) {
            const std::span<const uint32_t> corners(mesh.faces->corners);
            size_t offset = 0;
            for (const uint32_t size : mesh.faces->sizes) {
                if (offset + size > corners.size() || !add_face(corners.subspan(offset, size))) {
                    return std::nullopt;
                }
                offset += size;
            }
        } else {
            const std::span<const uint32_t> corners(*triangles);
            for (size_t offset = 0; offset + 3 <= corners.size(); offset += 3) {
                if (!add_face(corners.subspan(offset, 3))) {
                    return std::nullopt;
                }
            }
        }
        return cage;
    }

    size_t element_count(const Kakshya::DataVariant& values)
    {
        return std::visit([](const auto& held) { return held.size(); }, values);
    }

    class AssemblyArchive {
    public:
        AssemblyArchive(const Kakshya::MeshAssemblyContainer& assembly, std::filesystem::path file,
            std::string material_override)
            : m_assembly(assembly)
            , m_file(std::move(file))
            , m_override(std::move(material_override))
        {
        }

        bool write(std::string& error)
        {
            OArchive archive(Alembic::AbcCoreOgawa::WriteArchive(), m_file.string());
            OObject top = archive.getTop();
            const auto& data = m_assembly.get_data();
            const size_t rows = m_assembly.get_total_elements();

            std::vector<std::optional<Kakshya::MeshAssemblyPlacement>> placements(rows);
            std::vector<bool> has_children(rows, false);
            for (size_t row = 0; row < rows; ++row) {
                placements[row] = m_assembly.placement(row);
                if (placements[row] && placements[row]->parent && *placements[row]->parent < row) {
                    has_children[*placements[row]->parent] = true;
                }
            }

            std::unordered_map<uint64_t, const Kakshya::MeshAssemblyChannel*> channels;
            if (!data.motions.empty()) {
                for (const auto& channel : data.motions.front().channels) {
                    channels.emplace(channel.row, &channel);
                }
                if (data.motions.size() > 1) {
                    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                        "AlembicModelWriter: an archive holds one motion, '{}' is written and {} more are not",
                        data.motions.front().name, data.motions.size() - 1);
                }
                if (!data.motions.front().weights.empty()) {
                    MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                        "AlembicModelWriter: Alembic has no morph weights, they are not written");
                }
            }

            std::unordered_map<uint64_t, Metadata> metadata;
            Metadata scene_metadata;
            if (const auto* held = data.facets.get<Kakshya::MeshAssemblyMetadata>()) {
                for (const auto& entry : held->entries) {
                    if (entry.row) {
                        metadata[*entry.row].push_back(&entry);
                    } else {
                        scene_metadata.push_back(&entry);
                    }
                }
            }
            std::unordered_map<uint64_t, const Kakshya::MeshAssemblyVisibility::Entry*> visibility;
            if (const auto* held = data.facets.get<Kakshya::MeshAssemblyVisibility>()) {
                for (const auto& entry : held->entries) {
                    visibility.emplace(entry.row, &entry);
                }
            }
            write_user_values(top.getProperties(), scene_metadata);

            std::unordered_set<std::string> root_names;
            for (const auto& placement : placements) {
                if (placement && !placement->parent) {
                    root_names.insert(placement->name);
                }
            }
            write_materials(top, root_names);

            std::vector<OObject> objects(rows);
            for (size_t row = 0; row < rows; ++row) {
                const auto& placement = placements[row];
                if (!placement) {
                    objects[row] = top;
                    continue;
                }

                OObject parent = placement->parent && *placement->parent < row ? objects[*placement->parent] : top;
                const std::string fallback = "row_" + std::to_string(row);
                const auto channel = channels.find(row);
                const bool moves = channel != channels.end();
                const bool still = !moves && is_identity(placement->local);
                const bool shows = placement->source && *placement->source < data.sources.size();

                const auto held = metadata.find(row);
                const Metadata& user = held != metadata.end() ? held->second : m_none;
                const auto shown = visibility.find(row);
                OObject made;

                if (shows && still) {
                    const bool instanceable = !has_children[row] && user.empty() && shown == visibility.end();
                    const std::string name = m_names.child_of(parent, placement->name, fallback);
                    bool instanced = false;
                    if (!geometry(parent, name, *placement->source, placement->material, user,
                            instanceable, made, instanced, error)) {
                        return false;
                    }
                    objects[row] = made;
                    if (instanced) {
                        continue;
                    }
                } else {
                    const std::string name = m_names.child_of(parent, placement->name, fallback);
                    if (still) {
                        made = OObject(parent, name);
                        write_user_values(made.getProperties(), user);
                    } else {
                        OXform xform(parent, name);
                        write_transform(xform, placement->local, moves ? channel->second : nullptr);
                        if (!user.empty()) {
                            write_user_values(xform.getSchema().getUserProperties(), user);
                        }
                        made = xform;
                    }
                    objects[row] = made;

                    if (shows) {
                        const auto& source = data.sources[*placement->source];
                        const std::string child = m_names.child_of(made, source.name, "shape");
                        OObject shape;
                        bool instanced = false;
                        if (!geometry(made, child, *placement->source, placement->material, m_none,
                                true, shape, instanced, error)) {
                            return false;
                        }
                    }
                }

                if (shown != visibility.end() && made.valid()) {
                    write_visibility(made, *shown->second);
                }
            }
            return true;
        }

    private:
        void write_transform(OXform& xform, const glm::mat4& local, const Kakshya::MeshAssemblyChannel* channel)
        {
            auto& schema = xform.getSchema();
            std::vector<double> times;
            if (channel) {
                const auto gather = [&times](const auto& timed) {
                    for (size_t k = 0; timed && k < timed->size(); ++k) {
                        if (const auto time = timed->sampling().time(k)) {
                            times.push_back(*time);
                        }
                    }
                };
                gather(channel->translation);
                gather(channel->rotation);
                gather(channel->scale);
                std::ranges::sort(times);
                times.erase(std::ranges::unique(times).begin(), times.end());
            }

            if (times.empty()) {
                XformSample sample;
                sample.setMatrix(to_m44d(local));
                schema.set(sample);
                return;
            }

            const auto [rest_translation, rest_rotation, rest_scale] = Kinesis::decompose_transform(local).value_or(
                std::tuple { glm::vec3(0.0F), glm::quat(1.0F, 0.0F, 0.0F, 0.0F), glm::vec3(1.0F) });

            if (const auto sampling = acyclic(times)) {
                schema.setTimeSampling(sampling);
            }
            for (const double time : times) {
                glm::vec3 translation = rest_translation;
                glm::quat rotation = rest_rotation;
                glm::vec3 scale = rest_scale;
                if (channel->translation) {
                    translation = channel->translation->sample(time).value_or(translation);
                }
                if (channel->rotation) {
                    if (const auto q = channel->rotation->sample(time)) {
                        rotation = glm::quat(q->w, q->x, q->y, q->z);
                    }
                }
                if (channel->scale) {
                    scale = channel->scale->sample(time).value_or(scale);
                }
                XformSample sample;
                sample.setMatrix(to_m44d(Kinesis::compose_transform(translation, rotation, scale)));
                schema.set(sample);
            }
        }

        bool geometry(OObject& parent, const std::string& name, uint64_t index,
            const std::optional<uint64_t>& material, const Metadata& user, bool instanceable,
            OObject& made, bool& instanced, std::string& error)
        {
            const auto key = std::pair { index, material };
            if (instanceable) {
                if (const auto found = m_written.find(key); found != m_written.end()) {
                    instanced = parent.addChildInstance(found->second, name);
                    if (instanced) {
                        made = OObject();
                        return true;
                    }
                }
            }

            const auto& source = m_assembly.get_data().sources[index];
            MetaData md;
            if (const auto* tags = source.facets.get<std::unordered_map<std::string, std::string>>()) {
                for (const auto& [tag, value] : *tags) {
                    md.set(tag, value);
                }
            }

            OCompoundProperty user_compound;
            const bool wants_user = !user.empty();

            if (const auto* mesh = std::get_if<Kakshya::MeshData>(&source.geometry)) {
                if (!write_mesh(parent, name, md, index, *mesh, wants_user, made, user_compound, error)) {
                    return false;
                }
            } else if (const auto* spatial = std::get_if<Kakshya::SpatialData>(&source.geometry)) {
                write_spatial(parent, name, md, index, *spatial, wants_user, made, user_compound);
            } else if (const auto* lens = std::get_if<Kinesis::Lens>(&source.geometry)) {
                write_camera(parent, name, md, index, *lens, wants_user, made, user_compound);
            } else if (const auto* light = std::get_if<Kakshya::MeshAssemblyLight>(&source.geometry)) {
                write_light(parent, name, md, *light, wants_user, made, user_compound);
            }

            if (wants_user) {
                write_user_values(user_compound, user);
            }
            if (material && *material < m_material_paths.size()) {
                addMaterialAssignment(made, m_material_paths[*material]);
            }
            if (instanceable) {
                m_written.emplace(key, made);
            }
            return true;
        }

        bool write_mesh(OObject& parent, const std::string& name, const MetaData& md, uint64_t index,
            const Kakshya::MeshData& first, bool wants_user, OObject& made, OCompoundProperty& user,
            std::string& error)
        {
            const auto& source = m_assembly.get_data().sources[index];
            const auto frames = m_assembly.mesh_frames(index);
            const size_t count = frames ? std::max<size_t>(frames->count(), 1) : 1;
            const auto* map = source.facets.get<Kakshya::MeshAssemblyPointMap>();
            const auto* sets = source.facets.get<Kakshya::MeshAssemblyVertexSets>();
            const auto* attributes = source.facets.get<Kakshya::MeshAssemblyAttributes>();
            const auto* face_sets = source.facets.get<Kakshya::MeshAssemblyFaceSets>();
            const auto* subdivision = source.facets.get<Kakshya::MeshAssemblySubdivision>();
            const bool compact = !map || count == 1;

            if (m_assembly.skin(index) || source.facets.get<Kakshya::MeshAssemblyMorphs>()) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "AlembicModelWriter: '{}' has a skin or morph targets, only its base shape is written", name);
            }

            std::optional<OPolyMesh> poly;
            std::optional<OSubD> subd;
            if (subdivision) {
                subd.emplace(parent, name, md);
                made = *subd;
                if (wants_user) {
                    user = subd->getSchema().getUserProperties();
                }
            } else {
                poly.emplace(parent, name, md);
                made = *poly;
                if (wants_user) {
                    user = poly->getSchema().getUserProperties();
                }
            }
            if (frames && count > 1) {
                if (const auto sampling = times_of(frames->time_sampling(), count)) {
                    subd ? subd->getSchema().setTimeSampling(sampling) : poly->getSchema().setTimeSampling(sampling);
                }
            }

            std::unordered_map<std::string, GeomParamVariant> params;
            bool with_uvs = false;
            bool with_colors = false;
            bool with_weights = false;

            for (size_t s = 0; s < count; ++s) {
                const auto frame = frames ? frames->read(s) : nullptr;
                if (frames && !frame) {
                    error = "mesh '" + name + "': sample " + std::to_string(s) + " could not be read";
                    return false;
                }
                const auto cage = cage_of(frame ? *frame : first, map);
                if (!cage) {
                    error = "mesh '" + name + "' is invalid or not in the canonical MeshVertex layout";
                    return false;
                }

                if (s == 0) {
                    with_uvs = std::ranges::any_of(cage->vertices, [](const auto& v) { return v.uv != glm::vec2(0.0F); });
                    with_colors = (sets && !sets->colors.empty())
                        || std::ranges::any_of(cage->vertices, [](const auto& v) { return v.color != glm::vec4(1.0F); });
                    with_weights = std::ranges::any_of(cage->vertices, [](const auto& v) { return v.weight != 0.0F; });
                }

                const P3fArraySample positions(data_as<V3f>(cage->positions), cage->positions.size());
                const Int32ArraySample indices(cage->indices.data(), cage->indices.size());
                const Int32ArraySample counts(cage->counts.data(), cage->counts.size());

                const auto uvs = cage->encode(flipped(cage->column<glm::vec2>(&Kakshya::MeshVertex::uv)), compact);
                const auto normals = cage->encode(cage->column<glm::vec3>(&Kakshya::MeshVertex::normal), compact);

                std::vector<glm::vec3> velocities;
                if (attributes && count == 1) {
                    for (const auto& attribute : attributes->values) {
                        const auto* held = std::get_if<std::vector<glm::vec3>>(&attribute.values);
                        if (attribute.name == "velocity" && held && held->size() == cage->vertices.size()) {
                            velocities.assign(cage->positions.size(), glm::vec3(0.0F));
                            for (size_t v = 0; v < held->size(); ++v) {
                                velocities[cage->point_of[v]] = (*held)[v];
                            }
                        }
                    }
                }

                const auto fill = [&](auto& sample) {
                    sample.setPositions(positions);
                    sample.setFaceIndices(indices);
                    sample.setFaceCounts(counts);
                    if (with_uvs) {
                        sample.setUVs(OV2fGeomParam::Sample(
                            V2fArraySample(data_as<V2f>(uvs.first), uvs.first.size()), uvs.second));
                    }
                    if (!velocities.empty()) {
                        sample.setVelocities(V3fArraySample(data_as<V3f>(velocities), velocities.size()));
                    }
                };

                if (subd) {
                    OSubDSchema::Sample sample;
                    fill(sample);
                    sample.setSubdivisionScheme(subdivision->scheme);
                    sample.setFaceVaryingInterpolateBoundary(subdivision->face_varying_boundary);
                    sample.setFaceVaryingPropagateCorners(subdivision->face_varying_corners);
                    sample.setInterpolateBoundary(subdivision->boundary);
                    if (!subdivision->crease_indices.empty()) {
                        sample.setCreases(
                            Int32ArraySample(subdivision->crease_indices.data(), subdivision->crease_indices.size()),
                            Int32ArraySample(subdivision->crease_lengths.data(), subdivision->crease_lengths.size()),
                            FloatArraySample(subdivision->crease_sharpness.data(), subdivision->crease_sharpness.size()));
                    }
                    if (!subdivision->corner_indices.empty()) {
                        sample.setCorners(
                            Int32ArraySample(subdivision->corner_indices.data(), subdivision->corner_indices.size()),
                            FloatArraySample(subdivision->corner_sharpness.data(), subdivision->corner_sharpness.size()));
                    }
                    if (!subdivision->holes.empty()) {
                        sample.setHoles(Int32ArraySample(subdivision->holes.data(), subdivision->holes.size()));
                    }
                    subd->getSchema().set(sample);
                } else {
                    OPolyMeshSchema::Sample sample;
                    fill(sample);
                    sample.setNormals(ON3fGeomParam::Sample(
                        N3fArraySample(data_as<N3f>(normals.first), normals.first.size()), normals.second));
                    poly->getSchema().set(sample);
                }

                const OCompoundProperty arb = subd ? subd->getSchema().getArbGeomParams()
                                                   : poly->getSchema().getArbGeomParams();
                const auto put = [&](const std::string& param, auto encoded, bool as_color) {
                    const size_t size = encoded.first.size();
                    const Kakshya::DataVariant values { std::move(encoded.first) };
                    if (!set_attribute(params, arb, param, values, encoded.second, size, as_color)) {
                        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                            "AlembicModelWriter: '{}': attribute '{}' cannot be written", name, param);
                    }
                };

                if (with_colors) {
                    put("color", cage->encode(cage->column<glm::vec4>(&Kakshya::MeshVertex::color), compact), true);
                }
                if (s == 0 && sets) {
                    for (size_t i = 0; i < sets->colors.size(); ++i) {
                        if (sets->colors[i].size() == cage->vertices.size()) {
                            put("color" + std::to_string(i + 1), cage->encode(sets->colors[i], compact), true);
                        }
                    }
                    for (size_t i = 0; i < sets->uvs.size(); ++i) {
                        if (sets->uvs[i].size() == cage->vertices.size()) {
                            put("uv" + std::to_string(i + 1), cage->encode(flipped(sets->uvs[i]), compact), false);
                        }
                    }
                }
                if (with_weights) {
                    put(alembic::k_weight_param,
                        cage->encode(cage->column<float>(&Kakshya::MeshVertex::weight), compact), false);
                }
                if (s == 0 && attributes) {
                    for (const auto& attribute : attributes->values) {
                        if (attribute.name == "velocity" && !velocities.empty()) {
                            continue;
                        }
                        if (attribute.scope != Kakshya::SpatialScope::Varying) {
                            const size_t expected = attribute.scope == Kakshya::SpatialScope::Uniform
                                ? cage->counts.size()
                                : 1;
                            if (element_count(attribute.values) == expected) {
                                std::ignore = set_attribute(params, arb, attribute.name, attribute.values,
                                    alembic::to_alembic_scope(attribute.scope), expected, false);
                            }
                            continue;
                        }
                        std::visit(
                            [&](const auto& held) {
                                using Elem = typename std::remove_cvref_t<decltype(held)>::value_type;
                                if constexpr (std::same_as<Elem, float> || std::same_as<Elem, uint32_t>
                                    || std::same_as<Elem, glm::vec3> || std::same_as<Elem, glm::vec4>) {
                                    if (held.size() == cage->vertices.size()) {
                                        put(attribute.name, cage->encode(held, compact), false);
                                    }
                                }
                            },
                            attribute.values);
                    }
                }

                if (s == 0 && face_sets) {
                    for (const auto& set : face_sets->sets) {
                        const std::string set_name = m_names.child_of(made, set.name, "faces");
                        const std::vector<int32_t> faces(set.faces.begin(), set.faces.end());
                        OFaceSet& face_set = subd ? subd->getSchema().createFaceSet(set_name)
                                                  : poly->getSchema().createFaceSet(set_name);
                        face_set.getSchema().set(OFaceSetSchema::Sample(Int32ArraySample(faces.data(), faces.size())));
                        face_set.getSchema().setFaceExclusivity(
                            set.exclusive ? kFaceSetExclusive : kFaceSetNonExclusive);
                        if (set.material && *set.material < m_material_paths.size()) {
                            addMaterialAssignment(face_set, m_material_paths[*set.material]);
                        }
                    }
                }
            }
            return true;
        }

        void write_spatial(OObject& parent, const std::string& name, const MetaData& md, uint64_t index,
            const Kakshya::SpatialData& first, bool wants_user, OObject& made, OCompoundProperty& user)
        {
            const auto frames = m_assembly.spatial_frames(index);
            const size_t count = frames ? std::max<size_t>(frames->count(), 1) : 1;
            const auto* form = m_assembly.get_data().sources[index].facets.get<Kakshya::MeshAssemblyCurveForm>();
            const bool curves = first.topology == PrimitiveTopology::LINE_LIST
                || first.topology == PrimitiveTopology::LINE_STRIP;

            std::optional<OPoints> points;
            std::optional<OCurves> lines;
            if (curves) {
                lines.emplace(parent, name, md);
                made = *lines;
                if (wants_user) {
                    user = lines->getSchema().getUserProperties();
                }
            } else {
                points.emplace(parent, name, md);
                made = *points;
                if (wants_user) {
                    user = points->getSchema().getUserProperties();
                }
            }
            if (frames && count > 1) {
                if (const auto sampling = times_of(frames->time_sampling(), count)) {
                    curves ? lines->getSchema().setTimeSampling(sampling)
                           : points->getSchema().setTimeSampling(sampling);
                }
            }

            std::unordered_map<std::string, GeomParamVariant> params;
            for (size_t s = 0; s < count; ++s) {
                const auto frame = frames ? frames->read(s) : nullptr;
                const Kakshya::SpatialData& data = frame ? *frame : first;

                std::vector<int32_t> counts = data.vertex_counts_per_curve;
                if (curves && counts.empty()) {
                    if (data.topology == PrimitiveTopology::LINE_LIST) {
                        counts.assign(data.positions.size() / 2, 2);
                    } else {
                        counts.assign(1, static_cast<int32_t>(data.positions.size()));
                    }
                }

                const std::vector<float>* widths = nullptr;
                const std::vector<glm::vec2>* uvs = nullptr;
                const std::vector<glm::vec3>* normals = nullptr;
                GeometryScope width_scope = kVertexScope;
                GeometryScope uv_scope = kVertexScope;
                GeometryScope normal_scope = kVertexScope;
                std::vector<const Kakshya::SpatialAttribute*> rest;
                for (const auto& attribute : data.attributes) {
                    const auto scope = alembic::to_alembic_scope(attribute.scope);
                    if (attribute.name == "width" && std::holds_alternative<std::vector<float>>(attribute.values)) {
                        widths = &std::get<std::vector<float>>(attribute.values);
                        width_scope = scope;
                    } else if (curves && attribute.name == "uv"
                        && std::holds_alternative<std::vector<glm::vec2>>(attribute.values)) {
                        uvs = &std::get<std::vector<glm::vec2>>(attribute.values);
                        uv_scope = scope;
                    } else if (curves && attribute.name == "normal"
                        && std::holds_alternative<std::vector<glm::vec3>>(attribute.values)) {
                        normals = &std::get<std::vector<glm::vec3>>(attribute.values);
                        normal_scope = scope;
                    } else {
                        rest.push_back(&attribute);
                    }
                }

                const std::vector<glm::vec2> file_uvs = uvs ? flipped(*uvs) : std::vector<glm::vec2> {};
                const P3fArraySample positions(data_as<V3f>(data.positions), data.positions.size());
                const auto fill = [&](auto& sample) {
                    sample.setPositions(positions);
                    if (!data.velocities.empty()) {
                        sample.setVelocities(V3fArraySample(data_as<V3f>(data.velocities), data.velocities.size()));
                    }
                    if (widths) {
                        sample.setWidths(OFloatGeomParam::Sample(
                            FloatArraySample(widths->data(), widths->size()), width_scope));
                    }
                };

                if (curves) {
                    OCurvesSchema::Sample sample;
                    fill(sample);
                    sample.setCurvesNumVertices(Int32ArraySample(counts.data(), counts.size()));
                    sample.setType(form ? static_cast<CurveType>(form->type) : kLinear);
                    sample.setWrap(form && form->periodic ? kPeriodic : kNonPeriodic);
                    if (form) {
                        sample.setBasis(static_cast<BasisType>(form->basis));
                        if (!form->orders.empty()) {
                            sample.setOrders(UcharArraySample(form->orders.data(), form->orders.size()));
                        }
                        if (!form->knots.empty()) {
                            sample.setKnots(FloatArraySample(form->knots.data(), form->knots.size()));
                        }
                    }
                    if (uvs) {
                        sample.setUVs(OV2fGeomParam::Sample(V2fArraySample(data_as<V2f>(file_uvs), file_uvs.size()), uv_scope));
                    }
                    if (normals) {
                        sample.setNormals(ON3fGeomParam::Sample(
                            N3fArraySample(data_as<N3f>(*normals), normals->size()), normal_scope));
                    }
                    lines->getSchema().set(sample);
                } else {
                    OPointsSchema::Sample sample;
                    fill(sample);
                    if (!data.ids.empty()) {
                        sample.setIds(UInt64ArraySample(data.ids.data(), data.ids.size()));
                    }
                    points->getSchema().set(sample);
                }

                const OCompoundProperty arb = curves ? lines->getSchema().getArbGeomParams()
                                                     : points->getSchema().getArbGeomParams();
                for (const auto* attribute : rest) {
                    size_t expected = data.positions.size();
                    if (attribute->scope == Kakshya::SpatialScope::Constant) {
                        expected = 1;
                    } else if (attribute->scope == Kakshya::SpatialScope::Uniform) {
                        expected = curves ? counts.size() : 1;
                    }
                    if (!set_attribute(params, arb, attribute->name, attribute->values,
                            alembic::to_alembic_scope(attribute->scope), expected, attribute->name == "color")) {
                        MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                            "AlembicModelWriter: '{}': attribute '{}' cannot be written", name, attribute->name);
                    }
                }
            }
        }

        void write_camera(OObject& parent, const std::string& name, const MetaData& md, uint64_t index,
            const Kinesis::Lens& first, bool wants_user, OObject& made, OCompoundProperty& user)
        {
            OCamera camera(parent, name, md);
            made = camera;
            auto& schema = camera.getSchema();

            const auto frames = m_assembly.lens_frames(index);
            const size_t count = frames ? std::max<size_t>(frames->count(), 1) : 1;
            if (frames && count > 1) {
                if (const auto sampling = times_of(frames->time_sampling(), count)) {
                    schema.setTimeSampling(sampling);
                }
            }
            for (size_t s = 0; s < count; ++s) {
                const auto frame = frames ? frames->read(s) : nullptr;
                schema.set(camera_sample(frame ? *frame : first));
            }

            if (first.orthographic_width > 0.0) {
                ODoubleProperty(schema.getUserProperties(), alembic::k_orthographic_property)
                    .set(first.orthographic_width);
            }
            if (wants_user) {
                user = schema.getUserProperties();
            }
        }

        void write_light(OObject& parent, const std::string& name, const MetaData& md,
            const Kakshya::MeshAssemblyLight& light, bool wants_user, OObject& made, OCompoundProperty& user)
        {
            OLight object(parent, name, md);
            made = object;
            OCompoundProperty held = object.getSchema().getUserProperties();
            const std::string prefix = alembic::k_private_prefix;

            OInt32Property(held, prefix + "kind").set(static_cast<int32_t>(light.kind));
            OV3fProperty(held, prefix + "color").set(to_v3f(light.color));
            OV3fProperty(held, prefix + "specular").set(to_v3f(light.specular));
            OV3fProperty(held, prefix + "ambient").set(to_v3f(light.ambient));
            OV3fProperty(held, prefix + "attenuation").set(to_v3f(light.attenuation));
            OFloatProperty(held, prefix + "inner_cone").set(light.inner_cone);
            OFloatProperty(held, prefix + "outer_cone").set(light.outer_cone);
            OV2fProperty(held, prefix + "size").set(V2f(light.size.x, light.size.y));
            if (wants_user) {
                user = held;
            }
        }

        std::string texture_path(const Kakshya::MeshAssemblyTexture& texture)
        {
            if (!texture.image) {
                return texture.path;
            }
            if (const auto found = m_textures.find(texture.image.get()); found != m_textures.end()) {
                return found->second;
            }

            const auto bytes = STBImageWriter::encode_png(*texture.image);
            const std::string name = m_file.stem().string() + "_texture" + std::to_string(m_textures.size()) + ".png";
            std::ofstream out;
            if (!bytes.empty()) {
                out.open(m_file.parent_path() / name, std::ios::binary);
                out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            }
            if (bytes.empty() || !out) {
                MF_WARN(Journal::Component::IO, Journal::Context::FileIO,
                    "AlembicModelWriter: the pixels of '{}' could not be written as PNG", texture.path);
                return texture.path;
            }
            m_textures.emplace(texture.image.get(), name);
            return name;
        }

        void write_map(const OCompoundProperty& params, const std::string& prefix,
            const Kakshya::MeshAssemblyTexture& texture)
        {
            OStringProperty(params, prefix).set(texture_path(texture));

            const std::array<int32_t, 2> wrap {
                static_cast<int32_t>(texture.wrap_u), static_cast<int32_t>(texture.wrap_v)
            };
            OInt32ArrayProperty(params, prefix + "_wrap").set(Int32ArraySample(wrap.data(), wrap.size()));
            OInt32Property(params, prefix + "_uvSet").set(static_cast<int32_t>(texture.uv_set));
            if (texture.transform) {
                const std::array<float, 5> held {
                    texture.transform->offset.x, texture.transform->offset.y,
                    texture.transform->scale.x, texture.transform->scale.y, texture.transform->rotation
                };
                OFloatArrayProperty(params, prefix + "_transform").set(FloatArraySample(held.data(), held.size()));
            }
        }

        void write_surface(OMaterialSchema& schema, const Kakshya::MeshAssemblyMaterial& material)
        {
            const auto* surface = material.facets.get<Kakshya::MeshAssemblySurface>();
            Kakshya::MeshAssemblyMaps maps;
            if (const auto* held = material.facets.get<Kakshya::MeshAssemblyMaps>()) {
                maps = *held;
            }
            if (!maps.base_color && !material.diffuse_path.empty()) {
                maps.base_color = Kakshya::MeshAssemblyTexture { .path = material.diffuse_path };
            }

            const std::array<const std::optional<Kakshya::MeshAssemblyTexture>*, 7> slots {
                &maps.base_color, &maps.normal, &maps.metalness, &maps.roughness,
                &maps.emissive, &maps.occlusion, &maps.opacity
            };
            if (!surface && std::ranges::none_of(slots, [](const auto* slot) { return slot->has_value(); })) {
                return;
            }

            schema.setShader(alembic::k_material_target, alembic::k_surface_type, "standard");
            const OCompoundProperty params = schema.getShaderParameters(
                alembic::k_material_target, alembic::k_surface_type);

            if (surface) {
                const auto& c = surface->base_color;
                OC4fProperty(params, "baseColor").set(C4f(c.r, c.g, c.b, c.a));
                OFloatProperty(params, "metallic").set(surface->metallic);
                OFloatProperty(params, "roughness").set(surface->roughness);
                OC3fProperty(params, "emissive").set(C3f(surface->emissive.r, surface->emissive.g, surface->emissive.b));
                OFloatProperty(params, "alphaCutoff").set(surface->alpha_cutoff);
                OInt32Property(params, "alpha").set(static_cast<int32_t>(surface->alpha));
                OBoolProperty(params, "doubleSided").set(Alembic::Util::bool_t(surface->double_sided));
            }
            for (size_t i = 0; i < slots.size(); ++i) {
                if (*slots[i]) {
                    write_map(params, std::string(alembic::k_map_slots[i]) + "Map", **slots[i]);
                }
            }
        }

        /** Keys of the form "target/type/parameter" go back to the shader they were read from. */
        void write_properties(OMaterialSchema& schema, const Kakshya::MeshAssemblyProperties& properties)
        {
            std::map<std::pair<std::string, std::string>, std::string> shaders;
            for (const auto& entry : properties.entries) {
                const auto* shader = std::get_if<std::string>(&entry.value);
                const auto slash = entry.key.find('/');
                if (shader && slash != std::string::npos && entry.key.find('/', slash + 1) == std::string::npos) {
                    shaders[{ entry.key.substr(0, slash), entry.key.substr(slash + 1) }] = *shader;
                }
            }

            std::map<std::pair<std::string, std::string>, OCompoundProperty> compounds;
            std::unordered_set<std::string> used;
            const auto compound_of = [&](const std::string& target, const std::string& type) {
                const auto key = std::pair { target, type };
                if (const auto found = compounds.find(key); found != compounds.end()) {
                    return found->second;
                }
                const auto shader = shaders.find(key);
                schema.setShader(target, type, shader != shaders.end() ? shader->second : "standard");
                return compounds.emplace(key, schema.getShaderParameters(target, type)).first->second;
            };

            for (const auto& [key, shader] : shaders) {
                std::ignore = compound_of(key.first, key.second);
            }

            for (const auto& entry : properties.entries) {
                std::string target = alembic::k_material_target;
                std::string type = alembic::k_properties_type;
                std::string param = entry.key;

                const auto first = entry.key.find('/');
                const auto second = first == std::string::npos ? first : entry.key.find('/', first + 1);
                if (second != std::string::npos) {
                    target = entry.key.substr(0, first);
                    type = entry.key.substr(first + 1, second - first - 1);
                    param = entry.key.substr(second + 1);
                } else if (first != std::string::npos && std::holds_alternative<std::string>(entry.value)) {
                    continue;
                }
                std::ranges::replace(param, '/', '_');
                if (param.empty() || !used.insert(target + "/" + type + "/" + param).second) {
                    continue;
                }

                const OCompoundProperty params = compound_of(target, type);
                std::visit(
                    [&](const auto& value) {
                        using Value = std::remove_cvref_t<decltype(value)>;
                        if constexpr (std::same_as<Value, std::string>) {
                            OStringProperty(params, param).set(value);
                        } else if constexpr (std::same_as<Value, std::vector<float>>) {
                            OFloatArrayProperty(params, param).set(FloatArraySample(value.data(), value.size()));
                        } else if constexpr (std::same_as<Value, std::vector<double>>) {
                            ODoubleArrayProperty(params, param).set(DoubleArraySample(value.data(), value.size()));
                        } else if constexpr (std::same_as<Value, std::vector<int32_t>>) {
                            OInt32ArrayProperty(params, param).set(Int32ArraySample(value.data(), value.size()));
                        } else {
                            OUcharArrayProperty(params, param).set(UcharArraySample(value.data(), value.size()));
                        }
                    },
                    entry.value);
            }
        }

        void write_materials(OObject& top, const std::unordered_set<std::string>& root_names)
        {
            const auto& materials = m_assembly.get_data().materials;
            if (materials.empty()) {
                return;
            }

            std::string group_name = "materials";
            for (size_t n = 2; root_names.contains(group_name); ++n) {
                group_name = "materials_" + std::to_string(n);
            }
            OObject group(top, m_names.child_of(top, group_name, group_name));

            for (size_t i = 0; i < materials.size(); ++i) {
                const std::string name = m_names.child_of(group,
                    m_override.empty() ? materials[i].name : m_override, "material_" + std::to_string(i));
                OMaterial material(group, name);
                auto& schema = material.getSchema();
                write_surface(schema, materials[i]);
                if (const auto* properties = materials[i].facets.get<Kakshya::MeshAssemblyProperties>()) {
                    write_properties(schema, *properties);
                }
                m_material_paths.push_back("/" + group.getName() + "/" + name);
            }
        }

        const Kakshya::MeshAssemblyContainer& m_assembly;
        std::filesystem::path m_file;
        std::string m_override;
        Names m_names;
        const Metadata m_none;
        std::vector<std::string> m_material_paths;
        std::map<std::pair<uint64_t, std::optional<uint64_t>>, OObject> m_written;
        std::unordered_map<const Kakshya::ImageData*, std::string> m_textures;
    };

} // namespace

void AlembicModelWriter::register_with_registry()
{
    ModelWriterRegistry::instance().register_writer(
        { "abc" },
        []() -> std::unique_ptr<ModelWriter> {
            return std::make_unique<AlembicModelWriter>();
        });

    MF_INFO(Journal::Component::IO, Journal::Context::Init, "AlembicModelWriter registered for: abc");
}

bool AlembicModelWriter::can_write(const std::string& filepath) const
{
    auto extension = std::filesystem::path(filepath).extension().string();
    std::ranges::transform(extension, extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".abc";
}

bool AlembicModelWriter::write(
    const std::string& filepath,
    const std::vector<Kakshya::MeshData>& meshes,
    const ModelWriteOptions& options)
{
    m_last_error.clear();

    if (meshes.empty()) {
        m_last_error = "No meshes to write";
        return false;
    }

    auto assembly = std::make_shared<Kakshya::MeshAssemblyContainer>();
    for (size_t i = 0; i < meshes.size(); ++i) {
        if (!meshes[i].is_valid()) {
            m_last_error = "MeshData failed is_valid()";
            return false;
        }
        const std::string name = "mesh_" + std::to_string(i);
        Kakshya::MeshAssemblySource source;
        source.geometry = meshes[i];
        source.name = name;
        const auto index = assembly->add_source(std::move(source));
        if (!assembly->add_placement({ .name = name, .source = index })) {
            m_last_error = "Mesh " + std::to_string(i) + " could not be placed";
            return false;
        }
    }

    return write(filepath, std::shared_ptr<const Kakshya::MeshAssemblyContainer>(std::move(assembly)), options);
}

bool AlembicModelWriter::write(
    const std::string& filepath,
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
    const ModelWriteOptions& options)
{
    m_last_error.clear();

    if (!assembly || assembly->get_total_elements() == 0) {
        m_last_error = "No assembly to write";
        return false;
    }

    const auto resolved = resolve_write_path(filepath);
    try {
        AssemblyArchive archive(*assembly, resolved, options.material_name_override);
        if (!archive.write(m_last_error)) {
            MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
                "AlembicModelWriter: export failed for '{}': {}", resolved, m_last_error);
            return false;
        }
    } catch (const std::exception& e) {
        m_last_error = e.what();
        MF_ERROR(Journal::Component::IO, Journal::Context::FileIO,
            "AlembicModelWriter: export failed for '{}': {}", resolved, m_last_error);
        return false;
    }

    MF_INFO(Journal::Component::IO, Journal::Context::FileIO,
        "AlembicModelWriter: wrote '{}', {} row(s)", resolved, assembly->get_total_elements());
    return true;
}

} // namespace MayaFlux::IO
