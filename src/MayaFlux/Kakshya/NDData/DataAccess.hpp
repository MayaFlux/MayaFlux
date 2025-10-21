#pragma once

#include "NDData.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class DataAccess
 * @brief Type-erased accessor for NDData with semantic view construction
 *
 * Provides a unified interface to access data either as structured types
 * (glm::vec3, etc.) or as scalar arrays (double, float), based on modality
 * and user intent. Container classes remain template-free.
 */
class DataAccess {
public:
    DataAccess(DataVariant& variant,
        const std::vector<DataDimension>& dimensions,
        DataModality modality)
        : m_variant(variant)
        , m_dimensions(dimensions)
        , m_modality(modality)
    {
    }

    /**
     * @brief Get explicit typed view of data
     * @tparam T View type (glm::vec3, double, float, etc.)
     * @return Span-like view of data as type T
     *
     * @note If type conversion is needed, the view remains valid for the
     *       lifetime of this DataAccess object. Multiple calls with the
     *       same type reuse the cached conversion.
     */
    template <typename T>
    auto view() const;

    /**
     * @brief Get raw buffer info for GPU upload
     * @return Tuple of (void* data, size_t bytes, format_hint)
     */
    [[nodiscard]] auto gpu_buffer() const
    {
        return std::visit([&](auto& vec) {
            using T = typename std::decay_t<decltype(vec)>::value_type;
            return std::make_tuple(
                static_cast<const void*>(vec.data()),
                vec.size() * sizeof(T),
                get_format_hint<T>());
        },
            m_variant);
    }

    [[nodiscard]] DataModality modality() const { return m_modality; }

    [[nodiscard]] bool is_structured() const
    {
        return !m_dimensions.empty() && m_dimensions[0].grouping.has_value();
    }

    [[nodiscard]] size_t element_count() const
    {
        if (is_structured()) {
            return m_dimensions[0].size;
        }
        return std::visit([](const auto& vec) { return vec.size(); }, m_variant);
    }

    [[nodiscard]] size_t component_count() const
    {
        if (is_structured() && m_dimensions[0].grouping) {
            return m_dimensions[0].grouping->count;
        }
        return 1;
    }

    [[nodiscard]] std::string type_description() const
    {
        if (is_structured()) {
            return Journal::format("{}×{} ({})",
                element_count(),
                component_count(),
                modality_to_string(m_modality));
        }
        return Journal::format("scalar×{} ({})",
            element_count(),
            modality_to_string(m_modality));
    }

    /**
     * @brief Get suggested view type for this data's modality
     * @return String describing recommended view type
     */
    [[nodiscard]] std::string_view suggested_view_type() const
    {
        switch (m_modality) {
        case DataModality::VERTEX_POSITIONS_3D:
        case DataModality::VERTEX_NORMALS_3D:
        case DataModality::VERTEX_TANGENTS_3D:
        case DataModality::VERTEX_COLORS_RGB:
            return "glm::vec3";

        case DataModality::TEXTURE_COORDS_2D:
            return "glm::vec2";

        case DataModality::VERTEX_COLORS_RGBA:
            return "glm::vec4";

        case DataModality::TRANSFORMATION_MATRIX:
            return "glm::mat4";

        case DataModality::AUDIO_1D:
        case DataModality::AUDIO_MULTICHANNEL:
        case DataModality::SPECTRAL_2D:
            return "double";

        case DataModality::IMAGE_2D:
        case DataModality::IMAGE_COLOR:
        case DataModality::TEXTURE_2D:
            return "float";

        default:
            return "unknown";
        }
    }

private:
    DataVariant& m_variant;
    const std::vector<DataDimension>& m_dimensions;
    DataModality m_modality;

    mutable std::optional<std::vector<uint8_t>> m_conversion_cache;

    template <typename T>
    static uint32_t get_format_hint()
    {
        // Return VkFormat or similar enum based on type
        // Placeholder for now
        return 0;
    }

    template <typename T>
        requires std::is_arithmetic_v<T>
    std::span<const T> create_scalar_view() const;

    template <typename T>
    void validate_structured_access() const;

    /**
     * @brief Ensure conversion cache exists and is properly sized
     * @tparam T Target component type
     * @param required_bytes Size needed in bytes
     * @return Pointer to cache storage
     */
    template <typename T>
    void* ensure_conversion_cache(size_t required_bytes) const
    {
        if (!m_conversion_cache || m_conversion_cache->size() < required_bytes) {
            m_conversion_cache = std::vector<uint8_t>(required_bytes);
        }
        return m_conversion_cache->data();
    }
};

/**
 * @class StructuredView
 * @brief Span-like view that interprets flat data as structured types (glm::vec3, etc.)
 */
template <typename T>
    requires GlmType<T>
class StructuredView {
public:
    using value_type = T;
    using component_type = glm_component_type<T>;
    static constexpr size_t components = glm_component_count<T>();

    StructuredView(const void* data, size_t element_count, size_t stride_bytes = 0)
        : m_data(static_cast<const component_type*>(data))
        , m_element_count(element_count)
        , m_stride(stride_bytes == 0 ? components : stride_bytes / sizeof(component_type))
    {
    }

    T operator[](size_t idx) const
    {
        const component_type* base = m_data + (idx * m_stride);
        return construct_glm_type<T>(base);
    }

    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;

        iterator(const component_type* ptr, size_t stride)
            : m_ptr(ptr)
            , m_stride(stride)
        {
        }

        T operator*() const { return construct_glm_type<T>(m_ptr); }
        iterator& operator++()
        {
            m_ptr += m_stride;
            return *this;
        }
        iterator operator++(int)
        {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        bool operator==(const iterator& other) const { return m_ptr == other.m_ptr; }
        bool operator!=(const iterator& other) const { return m_ptr != other.m_ptr; }

    private:
        const component_type* m_ptr;
        size_t m_stride;
    };

    iterator begin() const { return iterator(m_data, m_stride); }
    iterator end() const { return iterator(m_data + (m_element_count * m_stride), m_stride); }

    [[nodiscard]] size_t size() const { return m_element_count; }

    // GPU upload: get raw pointer (zero-copy when stride is tight)
    [[nodiscard]] const void* data() const { return m_data; }
    [[nodiscard]] size_t size_bytes() const { return m_element_count * m_stride * sizeof(component_type); }

    std::vector<T> to_vector() const
    {
        std::vector<T> result;
        result.reserve(m_element_count);
        for (size_t i = 0; i < m_element_count; ++i) {
            result.push_back((*this)[i]);
        }
        return result;
    }

private:
    const component_type* m_data;
    size_t m_element_count;
    size_t m_stride; // In units of component_type

    template <typename GlmType>
    static GlmType construct_glm_type(const component_type* components)
    {
        if constexpr (GlmVec2Type<GlmType>) {
            return GlmType { components[0], components[1] };
        } else if constexpr (GlmVec3Type<GlmType>) {
            return GlmType { components[0], components[1], components[2] };
        } else if constexpr (GlmVec4Type<GlmType>) {
            return GlmType { components[0], components[1], components[2], components[3] };
        } else if constexpr (GlmMatrixType<GlmType>) {
            return glm::make_mat4(components);
        }
    }
};

// ============================================================================
// TEMPLATE IMPLEMENTATIONS
// ============================================================================

template <typename T>
auto DataAccess::view() const
{
    if constexpr (GlmType<T>) {
        validate_structured_access<T>();

        return std::visit([&](auto& vec) -> StructuredView<T> {
            using StorageType = typename std::decay_t<decltype(vec)>::value_type;
            using ComponentType = glm_component_type<T>;
            constexpr size_t components = glm_component_count<T>();

            size_t required_components = m_dimensions[0].size * components;
            if (vec.size() < required_components) {
                error<std::runtime_error>(
                    Journal::Component::Kakshya,
                    Journal::Context::Runtime,
                    std::source_location::current(),
                    "Insufficient data: need {} elements of type {} but have {} elements of type {}",
                    m_dimensions[0].size,
                    typeid(T).name(),
                    vec.size() / components,
                    typeid(StorageType).name());
            }

            if constexpr (std::is_same_v<StorageType, ComponentType>) {
                return StructuredView<T>(vec.data(), m_dimensions[0].size);
            } else if constexpr (std::is_convertible_v<StorageType, ComponentType>) {
                size_t required_bytes = vec.size() * sizeof(ComponentType);
                void* cache_ptr = ensure_conversion_cache<ComponentType>(required_bytes);

                ComponentType* cache_data = static_cast<ComponentType*>(cache_ptr);
                std::ranges::transform(vec, cache_data,
                    [](auto val) { return static_cast<ComponentType>(val); });

                return StructuredView<T>(cache_data, m_dimensions[0].size);
            } else {
                error<std::invalid_argument>(
                    Journal::Component::Kakshya,
                    Journal::Context::Runtime,
                    std::source_location::current(),
                    "Cannot convert storage type {} to component type {}",
                    typeid(StorageType).name(),
                    typeid(ComponentType).name());
            }
        },
            m_variant);

    } else if constexpr (std::is_arithmetic_v<T>) {
        return create_scalar_view<T>();

    } else {
        static_assert(always_false_v<T>,
            "Unsupported view type. Use glm types or arithmetic types (double, float, int, etc.)");
    }
}

template <typename T>
    requires std::is_arithmetic_v<T>
std::span<const T> DataAccess::create_scalar_view() const
{
    return std::visit([this](auto& vec) -> std::span<const T> {
        using StorageType = typename std::decay_t<decltype(vec)>::value_type;

        if constexpr (std::is_same_v<StorageType, T>) {
            return std::span<const T>(vec.data(), vec.size());
        } else if constexpr (std::is_convertible_v<StorageType, T>) {
            size_t required_bytes = vec.size() * sizeof(T);
            void* cache_ptr = ensure_conversion_cache<T>(required_bytes);

            T* cache_data = static_cast<T*>(cache_ptr);
            std::ranges::transform(vec, cache_data,
                [](auto val) { return static_cast<T>(val); });

            return std::span<const T>(cache_data, vec.size());
        } else {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Cannot convert storage type {} to requested type {}",
                typeid(StorageType).name(),
                typeid(T).name());
        }
    },
        m_variant);
}

template <typename T>
void DataAccess::validate_structured_access() const
{
    constexpr size_t requested_components = glm_component_count<T>();

    if (m_dimensions.empty()) {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cannot create structured view: no dimensions defined");
    }

    if (!m_dimensions[0].grouping) {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Cannot create structured view: dimension '{}' missing component grouping. "
            "Use DataDimension::grouped() to create structured dimensions.",
            m_dimensions[0].name);
    }

    size_t actual_components = m_dimensions[0].grouping->count;
    if (actual_components != requested_components) {
        error<std::runtime_error>(
            Journal::Component::Kakshya,
            Journal::Context::Runtime,
            std::source_location::current(),
            "Component count mismatch: requested {} components ({}), but data has {} components per element. "
            "Suggested type: {}",
            requested_components,
            typeid(T).name(),
            actual_components,
            suggested_view_type());
    }
}

} // namespace MayaFlux::Kakshya
