#include "FrameBlend.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include <glm/gtc/quaternion.hpp>

namespace MayaFlux::Kriya {

namespace {

    template <typename Frame>
    const WeightedFrame<Frame>& heaviest(std::span<const WeightedFrame<Frame>> picks)
    {
        return *std::ranges::max_element(picks, {}, &WeightedFrame<Frame>::weight);
    }

    template <typename Frame>
    double total_weight(std::span<const WeightedFrame<Frame>> picks)
    {
        double total = 0.0;
        for (const auto& pick : picks) {
            total += pick.weight;
        }
        return total;
    }

    template <typename Value>
    void accumulate(std::vector<Value>& into, const std::vector<Value>& from, double weight, const std::vector<size_t>* map = nullptr)
    {
        for (size_t i = 0; i < into.size(); ++i) {
            into[i] += static_cast<Value>(from[map ? (*map)[i] : i] * static_cast<float>(weight));
        }
    }

    bool same_lattice(const Kinesis::Lattice3D& a, const Kinesis::Lattice3D& b)
    {
        return a.resolution == b.resolution && a.bounds.min == b.bounds.min && a.bounds.max == b.bounds.max;
    }

    std::shared_ptr<const Kakshya::VolumeData> blend_volumes(
        const FrameContext&, std::span<const WeightedFrame<Kakshya::VolumeData>> picks)
    {
        const auto& base = heaviest(picks);
        const bool alike = std::ranges::all_of(picks, [&](const auto& pick) {
            return pick.frame && same_lattice(pick.frame->lattice, base.frame->lattice);
        });
        const double total = total_weight(picks);
        if (!alike || !(total > 0.0)) {
            return base.frame;
        }

        auto out = std::make_shared<Kakshya::VolumeData>(*base.frame);
        for (auto& field : out->fields) {
            if (auto* scalar = field.as_scalar()) {
                std::vector<float> mean(scalar->size(), 0.0F);
                double used = 0.0;
                for (const auto& pick : picks) {
                    const auto* other = pick.frame->find(field.name);
                    const auto* values = other ? other->as_scalar() : nullptr;
                    if (values && values->size() == mean.size()) {
                        accumulate(mean, *values, pick.weight);
                        used += pick.weight;
                    }
                }
                if (used > 0.0) {
                    for (float& value : mean) {
                        value = static_cast<float>(value / used);
                    }
                    *scalar = std::move(mean);
                }
            } else if (auto* vector = field.as_vector()) {
                std::vector<glm::vec3> mean(vector->size(), glm::vec3(0.0F));
                double used = 0.0;
                for (const auto& pick : picks) {
                    const auto* other = pick.frame->find(field.name);
                    const auto* values = other ? other->as_vector() : nullptr;
                    if (values && values->size() == mean.size()) {
                        accumulate(mean, *values, pick.weight);
                        used += pick.weight;
                    }
                }
                if (used > 0.0) {
                    for (auto& value : mean) {
                        value /= static_cast<float>(used);
                    }
                    *vector = std::move(mean);
                }
            }
        }
        return out;
    }

    glm::vec3 unit_or(const glm::vec3& v, const glm::vec3& fallback)
    {
        const float length = glm::length(v);
        return length > 1.0e-8F ? v / length : fallback;
    }

    std::shared_ptr<const Kakshya::MeshData> blend_meshes(
        const FrameContext&, std::span<const WeightedFrame<Kakshya::MeshData>> picks)
    {
        const auto& base = heaviest(picks);
        const auto* base_vertices = base.frame ? std::get_if<std::vector<uint8_t>>(&base.frame->vertex_variant) : nullptr;
        const auto* base_indices = base.frame ? std::get_if<std::vector<uint32_t>>(&base.frame->index_variant) : nullptr;
        const double total = total_weight(picks);
        if (!base_vertices || !base_indices || !(total > 0.0) || base.frame->layout.stride_bytes != sizeof(Kakshya::MeshVertex)) {
            return base.frame;
        }

        std::vector<const std::vector<uint8_t>*> sources;
        for (const auto& pick : picks) {
            const auto* vertices = pick.frame ? std::get_if<std::vector<uint8_t>>(&pick.frame->vertex_variant) : nullptr;
            const auto* indices = pick.frame ? std::get_if<std::vector<uint32_t>>(&pick.frame->index_variant) : nullptr;
            if (!vertices || !indices || vertices->size() != base_vertices->size() || *indices != *base_indices
                || pick.frame->layout.stride_bytes != sizeof(Kakshya::MeshVertex)) {
                return base.frame;
            }
            sources.push_back(vertices);
        }

        constexpr size_t floats = sizeof(Kakshya::MeshVertex) / sizeof(float);
        const size_t count = base_vertices->size() / sizeof(Kakshya::MeshVertex);
        std::vector<Kakshya::MeshVertex> blended(count);
        for (size_t v = 0; v < count; ++v) {
            std::array<double, floats> sum {};
            for (size_t p = 0; p < picks.size(); ++p) {
                std::array<float, floats> raw {};
                std::memcpy(raw.data(), sources[p]->data() + v * sizeof(Kakshya::MeshVertex), sizeof(Kakshya::MeshVertex));
                for (size_t k = 0; k < floats; ++k) {
                    sum[k] += static_cast<double>(raw[k]) * picks[p].weight;
                }
            }
            std::array<float, floats> mean {};
            for (size_t k = 0; k < floats; ++k) {
                mean[k] = static_cast<float>(sum[k] / total);
            }
            std::memcpy(&blended[v], mean.data(), sizeof(Kakshya::MeshVertex));

            Kakshya::MeshVertex original {};
            std::memcpy(&original, base_vertices->data() + v * sizeof(Kakshya::MeshVertex), sizeof(Kakshya::MeshVertex));
            blended[v].normal = unit_or(blended[v].normal, original.normal);
            const glm::vec3 direction = unit_or(glm::vec3(blended[v].tangent), glm::vec3(original.tangent));
            blended[v].tangent = glm::vec4(direction, original.tangent.w);
        }

        auto out = std::make_shared<Kakshya::MeshData>(*base.frame);
        auto& bytes = std::get<std::vector<uint8_t>>(out->vertex_variant);
        std::memcpy(bytes.data(), blended.data(), bytes.size());
        return out;
    }

    struct Alignment {
        bool ok { false };
        std::vector<std::vector<size_t>> maps;
    };

    Alignment align_points(std::span<const WeightedFrame<Kakshya::SpatialData>> picks, size_t base_index)
    {
        const auto& base = *picks[base_index].frame;
        Alignment alignment;
        alignment.maps.resize(picks.size());

        for (size_t p = 0; p < picks.size(); ++p) {
            const auto& frame = *picks[p].frame;
            if (frame.topology != base.topology || frame.positions.size() != base.positions.size()
                || frame.vertex_counts_per_curve != base.vertex_counts_per_curve) {
                return alignment;
            }

            auto& map = alignment.maps[p];
            map.resize(base.positions.size());
            std::iota(map.begin(), map.end(), size_t { 0 });

            const bool both_ids = !base.ids.empty() && frame.ids.size() == base.ids.size();
            if (both_ids && frame.ids != base.ids) {
                std::unordered_map<uint64_t, size_t> where;
                where.reserve(frame.ids.size());
                for (size_t i = 0; i < frame.ids.size(); ++i) {
                    where.emplace(frame.ids[i], i);
                }
                for (size_t i = 0; i < base.ids.size(); ++i) {
                    const auto it = where.find(base.ids[i]);
                    if (it == where.end()) {
                        return alignment;
                    }
                    map[i] = it->second;
                }
            }
        }
        alignment.ok = true;
        return alignment;
    }

    template <typename Value>
    std::vector<Value> mean_of(std::span<const WeightedFrame<Kakshya::SpatialData>> picks,
        const std::vector<std::vector<size_t>>* maps, double total,
        const std::function<const std::vector<Value>*(const Kakshya::SpatialData&)>& read)
    {
        const auto* first = read(*picks.front().frame);
        std::vector<Value> mean(first ? first->size() : 0, Value {});
        for (size_t p = 0; p < picks.size(); ++p) {
            const auto* values = read(*picks[p].frame);
            if (!values || values->size() != mean.size()) {
                return {};
            }
            accumulate(mean, *values, picks[p].weight, maps ? &(*maps)[p] : nullptr);
        }
        for (auto& value : mean) {
            value = static_cast<Value>(value / static_cast<float>(total));
        }
        return mean;
    }

    std::shared_ptr<const Kakshya::SpatialData> blend_points(
        const FrameContext&, std::span<const WeightedFrame<Kakshya::SpatialData>> picks)
    {
        const size_t base_index = static_cast<size_t>(std::ranges::max_element(picks, {}, &WeightedFrame<Kakshya::SpatialData>::weight) - picks.begin());
        const auto& base = picks[base_index];
        const double total = total_weight(picks);
        if (!(total > 0.0) || std::ranges::any_of(picks, [](const auto& pick) { return !pick.frame; })) {
            return base.frame;
        }

        // Reorder so the base frame comes first: the means read the first frame's sizes.
        std::vector<WeightedFrame<Kakshya::SpatialData>> ordered(picks.begin(), picks.end());
        std::swap(ordered[0], ordered[base_index]);
        const auto alignment = align_points(ordered, 0);
        if (!alignment.ok) {
            return base.frame;
        }

        auto out = std::make_shared<Kakshya::SpatialData>(*base.frame);
        out->positions = mean_of<glm::vec3>(ordered, &alignment.maps, total,
            [](const Kakshya::SpatialData& frame) { return &frame.positions; });

        const bool velocities = std::ranges::all_of(ordered, [&](const auto& pick) {
            return pick.frame->velocities.size() == out->positions.size() && !out->positions.empty();
        });
        if (velocities) {
            out->velocities = mean_of<glm::vec3>(ordered, &alignment.maps, total,
                [](const Kakshya::SpatialData& frame) { return &frame.velocities; });
        }

        for (auto& attribute : out->attributes) {
            const bool varying = attribute.scope == Kakshya::SpatialScope::Varying;
            const auto find = [&](const Kakshya::SpatialData& frame) -> const Kakshya::SpatialAttribute* {
                const auto it = std::ranges::find_if(frame.attributes, [&](const auto& other) {
                    return other.name == attribute.name && other.scope == attribute.scope
                        && other.values.index() == attribute.values.index();
                });
                return it == frame.attributes.end() ? nullptr : &*it;
            };
            if (!std::ranges::all_of(ordered, [&](const auto& pick) { return find(*pick.frame) != nullptr; })) {
                continue;
            }

            const auto* maps = varying ? &alignment.maps : nullptr;
            const auto blend = [&]<typename Value>() {
                const auto* base_values = std::get_if<std::vector<Value>>(&attribute.values);
                if (!base_values) {
                    return;
                }
                std::vector<Value> mean(base_values->size(), Value {});
                for (size_t p = 0; p < ordered.size(); ++p) {
                    const auto* held = find(*ordered[p].frame);
                    const auto* values = std::get_if<std::vector<Value>>(&held->values);
                    if (!values || values->size() != mean.size()) {
                        return;
                    }
                    accumulate(mean, *values, ordered[p].weight, maps ? &(*maps)[p] : nullptr);
                }
                for (auto& value : mean) {
                    value = static_cast<Value>(value / static_cast<float>(total));
                }
                attribute.values = std::move(mean);
            };

            switch (attribute.values.index()) {
            case 1:
                blend.template operator()<float>();
                break;
            case 7:
                blend.template operator()<glm::vec2>();
                break;
            case 8:
                blend.template operator()<glm::vec3>();
                break;
            case 9:
                blend.template operator()<glm::vec4>();
                break;
            default:
                break;
            }
        }
        return out;
    }

    struct Parts {
        glm::vec3 translation;
        glm::vec3 scale;
        glm::quat rotation;
    };

    Parts decompose(const glm::mat4& m)
    {
        Parts parts;
        parts.translation = glm::vec3(m[3]);
        glm::vec3 x(m[0]);
        glm::vec3 y(m[1]);
        glm::vec3 z(m[2]);
        parts.scale = { glm::length(x), glm::length(y), glm::length(z) };
        if (glm::dot(glm::cross(x, y), z) < 0.0F) {
            parts.scale.x = -parts.scale.x;
        }
        const glm::vec3 safe = glm::max(glm::abs(parts.scale), glm::vec3(1.0e-8F));
        const glm::mat3 basis(x / (parts.scale.x != 0.0F ? parts.scale.x : safe.x), y / safe.y, z / safe.z);
        parts.rotation = glm::normalize(glm::quat_cast(basis));
        return parts;
    }

    glm::mat4 compose(const Parts& parts)
    {
        glm::mat4 m = glm::mat4_cast(parts.rotation);
        m[0] *= parts.scale.x;
        m[1] *= parts.scale.y;
        m[2] *= parts.scale.z;
        m[3] = glm::vec4(parts.translation, 1.0F);
        return m;
    }

} // namespace

FrameOperation<Kakshya::VolumeData> volume_blend()
{
    return FrameOperation<Kakshya::VolumeData>::mix(blend_volumes);
}

FrameOperation<Kakshya::MeshData> mesh_blend()
{
    return FrameOperation<Kakshya::MeshData>::mix(blend_meshes);
}

FrameOperation<Kakshya::SpatialData> spatial_blend()
{
    return FrameOperation<Kakshya::SpatialData>::mix(blend_points);
}

FrameOperation<Kakshya::MeshAssemblyLocals> placement_blend(
    const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly)
{
    return FrameOperation<Kakshya::MeshAssemblyLocals>::mix(
        [assembly](const FrameContext&, std::span<const WeightedFrame<Kakshya::MeshAssemblyLocals>> picks)
            -> std::shared_ptr<const Kakshya::MeshAssemblyLocals> {
            const auto& base = heaviest(picks);
            if (!base.frame) {
                return nullptr;
            }

            const size_t rows = base.frame->matrices.size();
            std::vector<const WeightedFrame<Kakshya::MeshAssemblyLocals>*> used;
            double total = 0.0;
            for (const auto& pick : picks) {
                if (pick.frame && pick.frame->matrices.size() == rows && pick.weight > 0.0) {
                    used.push_back(&pick);
                    total += pick.weight;
                }
            }
            if (used.size() < 2 || !(total > 0.0)) {
                return base.frame;
            }

            std::vector<glm::mat4> locals(rows);
            for (size_t row = 0; row < rows; ++row) {
                const Parts anchor = decompose(base.frame->matrices[row]);
                glm::vec3 translation(0.0F);
                glm::vec3 scale(0.0F);
                glm::vec4 rotation(0.0F);
                for (const auto* pick : used) {
                    const Parts parts = decompose(pick->frame->matrices[row]);
                    const auto w = static_cast<float>(pick->weight / total);
                    translation += parts.translation * w;
                    scale += parts.scale * w;
                    glm::quat q = parts.rotation;
                    if (glm::dot(q, anchor.rotation) < 0.0F) {
                        q = -q;
                    }
                    rotation += glm::vec4(q.x, q.y, q.z, q.w) * w;
                }
                const float length = glm::length(rotation);
                const glm::quat blended = length > 1.0e-8F
                    ? glm::quat(rotation.w / length, rotation.x / length, rotation.y / length, rotation.z / length)
                    : anchor.rotation;
                locals[row] = compose({ .translation = translation, .scale = scale, .rotation = blended });
            }

            std::vector<std::vector<float>> weights = base.frame->weights;
            for (size_t row = 0; row < weights.size(); ++row) {
                auto& mixed = weights[row];
                std::vector<float> sum(mixed.size(), 0.0F);
                for (const auto* pick : used) {
                    const auto w = static_cast<float>(pick->weight / total);
                    const auto& other = row < pick->frame->weights.size() && pick->frame->weights[row].size() == mixed.size()
                        ? pick->frame->weights[row]
                        : mixed;
                    for (size_t t = 0; t < mixed.size(); ++t) {
                        sum[t] += other[t] * w;
                    }
                }
                mixed = std::move(sum);
            }

            auto blended = assembly
                ? Kakshya::resolve_world(*assembly, std::move(locals))
                : Kakshya::MeshAssemblyLocals { .matrices = std::move(locals), .world = {} };
            blended.weights = std::move(weights);
            return std::make_shared<const Kakshya::MeshAssemblyLocals>(std::move(blended));
        });
}

} // namespace MayaFlux::Kriya
