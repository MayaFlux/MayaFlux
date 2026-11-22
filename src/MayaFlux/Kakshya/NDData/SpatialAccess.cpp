#include "SpatialAccess.hpp"

namespace MayaFlux::Kakshya {

namespace {

    /**
     * @brief Spread an attribute's values over every vertex of the frame.
     * @return One value per vertex, or an empty vector when the attribute holds another
     *         type than T or its size does not match its scope.
     */
    template <typename T>
    std::vector<T> per_vertex(const SpatialData& frame, const SpatialAttribute& attribute)
    {
        const auto* values = std::get_if<std::vector<T>>(&attribute.values);
        if (!values) {
            return {};
        }

        const size_t count = frame.positions.size();
        const bool curves = !frame.vertex_counts_per_curve.empty();

        const SpatialScope scope = (attribute.scope == SpatialScope::Uniform && !curves)
            ? SpatialScope::Constant
            : attribute.scope;

        if (scope == SpatialScope::Varying) {
            return values->size() == count ? *values : std::vector<T> {};
        }

        if (scope == SpatialScope::Constant) {
            return values->size() == 1 ? std::vector<T>(count, values->front()) : std::vector<T> {};
        }

        if (values->size() != frame.vertex_counts_per_curve.size()) {
            return {};
        }

        std::vector<T> spread;
        spread.reserve(count);
        for (size_t curve = 0; curve < values->size(); ++curve) {
            const auto length = static_cast<size_t>(std::max(frame.vertex_counts_per_curve[curve], 0));
            spread.insert(spread.end(), length, (*values)[curve]);
        }
        return spread.size() == count ? spread : std::vector<T> {};
    }

    const SpatialAttribute* find_attribute(const SpatialData& frame, std::string_view name)
    {
        const auto it = std::ranges::find_if(frame.attributes,
            [name](const SpatialAttribute& attribute) { return attribute.name == name; });
        return it == frame.attributes.end() ? nullptr : &*it;
    }

    /** @brief The first of the given types the attribute holds per vertex, as a channel. */
    template <typename First, typename... Rest>
    std::optional<DataVariant> channel_of(const SpatialData& frame, const SpatialAttribute* attribute)
    {
        if (!attribute) {
            return std::nullopt;
        }
        if (auto values = per_vertex<First>(frame, *attribute); !values.empty()) {
            return DataVariant { std::move(values) };
        }
        if constexpr (sizeof...(Rest) > 0) {
            return channel_of<Rest...>(frame, attribute);
        } else {
            return std::nullopt;
        }
    }

    template <typename T>
    DataVariant constant_channel(size_t count, const T& value)
    {
        return DataVariant { std::vector<T>(count, value) };
    }

} // namespace

std::optional<VertexAccess> as_spatial_vertex_access(
    const SpatialData& frame,
    const VertexAccessConfig& config)
{
    const size_t count = frame.positions.size();
    if (count == 0) {
        return std::nullopt;
    }

    const bool curves = !frame.vertex_counts_per_curve.empty();

    std::array<std::optional<DataVariant>, 5> columns {
        channel_of<glm::vec4, glm::vec3>(frame, find_attribute(frame, "color")),
        channel_of<float>(frame, find_attribute(frame, "scalar")),
        channel_of<glm::vec2>(frame, find_attribute(frame, "uv")),
        channel_of<glm::vec3>(frame, find_attribute(frame, "normal")),
        channel_of<glm::vec4, glm::vec3>(frame, find_attribute(frame, "tangent")),
    };

    size_t used = 0;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i]) {
            used = i + 1;
        }
    }

    std::vector<DataVariant> channels;
    channels.reserve(used + 1);
    channels.emplace_back(frame.positions);

    for (size_t i = 0; i < used; ++i) {
        if (columns[i]) {
            channels.push_back(std::move(*columns[i]));
            continue;
        }
        switch (i) {
        case 0:
            channels.push_back(constant_channel(count, config.default_color));
            break;
        case 1:
            channels.push_back(constant_channel(count, curves ? config.default_thickness : config.default_size));
            break;
        case 2:
            channels.push_back(constant_channel(count, config.default_uv));
            break;
        case 3:
            channels.push_back(constant_channel(count, config.default_normal));
            break;
        default:
            channels.push_back(constant_channel(count, glm::vec4(config.default_tangent, 1.0F)));
            break;
        }
    }

    return curves ? as_line_vertex_access(channels, config) : as_point_vertex_access(channels, config);
}

std::vector<uint32_t> spatial_cluster_ids(const SpatialData& frame)
{
    if (const auto* attribute = find_attribute(frame, "cluster")) {
        if (auto ids = per_vertex<uint32_t>(frame, *attribute); !ids.empty()) {
            return ids;
        }
    }
    return std::vector<uint32_t>(frame.positions.size(), 0U);
}

} // namespace MayaFlux::Kakshya
