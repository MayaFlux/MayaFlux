#pragma once

#include "NDData.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace MayaFlux::Kakshya {

/**
 * @class DataInsertion
 * @brief Type-erased writer for NDData with semantic construction
 *
 * Companion to DataAccess for write operations. Provides unified interface
 * to insert data from various sources (GLM types, scalar arrays, etc.) into
 * DataVariant storage while maintaining appropriate dimension metadata.
 *
 * Design principle: All types are equal. GLM specializations exist for
 * ergonomics, not privileged status.
 */
class MAYALFUX_API DataInsertion {
public:
    DataInsertion(DataVariant& variant,
        std::vector<DataDimension>& dimensions,
        DataModality& modality)
        : m_variant(variant)
        , m_dimensions(dimensions)
        , m_modality(modality)
    {
    }

    /**
     * @brief Insert scalar array data
     * @tparam T Scalar arithmetic type (float, double, int, etc.)
     * @param data Source data
     * @param target_modality Semantic interpretation of data
     * @param replace_existing If true, replaces existing data; if false, appends
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    void insert_scalar(std::vector<T> data,
        DataModality target_modality,
        bool replace_existing = true)
    {
        validate_scalar_insertion(target_modality);

        if (replace_existing) {
            m_variant = std::move(data);
            m_modality = target_modality;

            m_dimensions.clear();
            m_dimensions.emplace_back(
                modality_to_dimension_name(target_modality),
                static_cast<uint64_t>(data.size()),
                1,
                modality_to_role(target_modality));
        } else {
            append_to_existing(std::move(data));
        }
    }

    /**
     * @brief Insert structured GLM data with automatic dimension setup
     * @tparam T GLM type (glm::vec2, glm::vec3, glm::vec4, glm::mat4)
     * @param data Source GLM elements
     * @param target_modality Semantic interpretation (must match GLM component count)
     * @param replace_existing If true, replaces existing data; if false, appends
     */
    template <GlmType T>
    void insert_structured(std::vector<T> data,
        DataModality target_modality,
        bool replace_existing = true)
    {
        validate_structured_insertion<T>(target_modality);

        if (replace_existing) {
            m_variant = std::move(data);
            m_modality = target_modality;

            // Create grouped dimension
            m_dimensions.clear();
            m_dimensions.push_back(create_structured_dimension<T>(
                static_cast<uint64_t>(data.size()),
                target_modality));
        } else {
            append_structured_to_existing(std::move(data));
        }
    }

    /**
     * @brief Insert data from span (copy operation)
     * @tparam T Element type
     * @param data_span Source span
     * @param target_modality Semantic interpretation
     */
    template <typename T>
    void insert_from_span(std::span<const T> data_span, DataModality target_modality)
    {
        std::vector<T> data_copy(data_span.begin(), data_span.end());

        if constexpr (GlmType<T>) {
            insert_structured(std::move(data_copy), target_modality);
        } else {
            insert_scalar(std::move(data_copy), target_modality);
        }
    }

    /**
     * @brief Convert and insert from different type
     * @tparam From Source type
     * @tparam To Target type
     * @param source Source data
     * @param target_modality Semantic interpretation
     */
    template <typename From, typename To>
    void insert_converted(const std::vector<From>& source, DataModality target_modality)
    {
        std::vector<To> converted;
        converted.reserve(source.size());

        if constexpr (std::is_arithmetic_v<From> && std::is_arithmetic_v<To>) {
            std::ranges::transform(source, std::back_inserter(converted),
                [](From val) { return static_cast<To>(val); });
        } else if constexpr (std::is_same_v<From, std::complex<float>> || std::is_same_v<From, std::complex<double>>) {
            std::ranges::transform(source, std::back_inserter(converted),
                [](From val) { return static_cast<To>(std::abs(val)); });
        } else {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Unsupported conversion from {} to {}",
                typeid(From).name(),
                typeid(To).name());
        }

        insert_scalar(std::move(converted), target_modality);
    }

    /**
     * @brief Reserve space without initialization
     * @param element_count Number of elements
     * @param target_modality Determines storage type
     */
    void reserve_space(size_t element_count, DataModality target_modality)
    {
        // Determine appropriate type based on modality
        if (is_structured_modality(target_modality)) {
            switch (target_modality) {
            case DataModality::VERTEX_POSITIONS_3D:
            case DataModality::VERTEX_NORMALS_3D:
            case DataModality::VERTEX_TANGENTS_3D:
            case DataModality::VERTEX_COLORS_RGB:
                m_variant = std::vector<glm::vec3>();
                std::get<std::vector<glm::vec3>>(m_variant).reserve(element_count);
                break;

            case DataModality::TEXTURE_COORDS_2D:
                m_variant = std::vector<glm::vec2>();
                std::get<std::vector<glm::vec2>>(m_variant).reserve(element_count);
                break;

            case DataModality::VERTEX_COLORS_RGBA:
                m_variant = std::vector<glm::vec4>();
                std::get<std::vector<glm::vec4>>(m_variant).reserve(element_count);
                break;

            case DataModality::TRANSFORMATION_MATRIX:
                m_variant = std::vector<glm::mat4>();
                std::get<std::vector<glm::mat4>>(m_variant).reserve(element_count);
                break;

            default:
                error<std::invalid_argument>(
                    Journal::Component::Kakshya,
                    Journal::Context::Runtime,
                    std::source_location::current(),
                    "Modality {} does not represent structured GLM data",
                    modality_to_string(target_modality));
            }
        } else {
            // Default to double for scalar modalities
            m_variant = std::vector<double>();
            std::get<std::vector<double>>(m_variant).reserve(element_count);
        }

        m_modality = target_modality;
    }

    /**
     * @brief Clear all data while preserving modality
     */
    void clear_data()
    {
        std::visit([](auto& vec) { vec.clear(); }, m_variant);
        m_dimensions.clear();
    }

    [[nodiscard]] DataModality current_modality() const { return m_modality; }

    [[nodiscard]] bool is_empty() const
    {
        return std::visit([](const auto& vec) { return vec.empty(); }, m_variant);
    }

private:
    DataVariant& m_variant;
    std::vector<DataDimension>& m_dimensions;
    DataModality& m_modality;

    /**
     * @brief Validate scalar insertion matches modality expectations
     */
    void validate_scalar_insertion(DataModality modality) const
    {
        if (is_structured_modality(modality)) {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Modality {} expects structured data (GLM types), not scalars. "
                "Use insert_structured() or change modality.",
                modality_to_string(modality));
        }
    }

    /**
     * @brief Validate structured insertion matches GLM component count
     */
    template <GlmType T>
    void validate_structured_insertion(DataModality modality) const
    {
        constexpr size_t components = glm_component_count<T>();

        bool valid = false;
        switch (modality) {
        case DataModality::VERTEX_POSITIONS_3D:
        case DataModality::VERTEX_NORMALS_3D:
        case DataModality::VERTEX_TANGENTS_3D:
        case DataModality::VERTEX_COLORS_RGB:
            valid = (components == 3);
            break;

        case DataModality::TEXTURE_COORDS_2D:
            valid = (components == 2);
            break;

        case DataModality::VERTEX_COLORS_RGBA:
            valid = (components == 4);
            break;

        case DataModality::TRANSFORMATION_MATRIX:
            valid = (components == 16);
            break;

        default:
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Modality {} does not represent structured GLM data",
                modality_to_string(modality));
        }

        if (!valid) {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "GLM type component count ({}) doesn't match modality {}. "
                "Suggested type: {}",
                components,
                modality_to_string(modality),
                suggest_glm_type_for_modality(modality));
        }
    }

    /**
     * @brief Create dimension descriptor for structured data
     */
    template <GlmType T>
    [[nodiscard]] DataDimension create_structured_dimension(uint64_t element_count,
        DataModality modality) const
    {
        // Use the static factory methods where available
        switch (modality) {
        case DataModality::VERTEX_POSITIONS_3D:
            return DataDimension::vertex_positions(element_count);

        case DataModality::VERTEX_NORMALS_3D:
            return DataDimension::vertex_normals(element_count);

        case DataModality::TEXTURE_COORDS_2D:
            return DataDimension::texture_coords(element_count);

        case DataModality::VERTEX_COLORS_RGB:
            return DataDimension::vertex_colors(element_count, false);

        case DataModality::VERTEX_COLORS_RGBA:
            return DataDimension::vertex_colors(element_count, true);

        case DataModality::TRANSFORMATION_MATRIX:
        case DataModality::VERTEX_TANGENTS_3D:
        default: {
            // Use grouped factory for other structured types
            constexpr size_t components = glm_component_count<T>();
            return DataDimension::grouped(
                modality_to_dimension_name(modality),
                element_count,
                static_cast<uint8_t>(components),
                modality_to_role(modality));
        }
        }
    }

    /**
     * @brief Append scalar data to existing storage
     */
    template <typename T>
    void append_to_existing(std::vector<T> new_data)
    {
        std::visit([&](auto& existing_vec) {
            using ExistingType = typename std::decay_t<decltype(existing_vec)>::value_type;

            if constexpr (std::is_same_v<ExistingType, T>) {
                existing_vec.insert(existing_vec.end(),
                    new_data.begin(),
                    new_data.end());
            } else if constexpr (std::is_arithmetic_v<ExistingType> && std::is_arithmetic_v<T>) {
                // Convert and append
                for (const auto& val : new_data) {
                    existing_vec.push_back(static_cast<ExistingType>(val));
                }
            } else {
                error<std::invalid_argument>(
                    Journal::Component::Kakshya,
                    Journal::Context::Runtime,
                    std::source_location::current(),
                    "Cannot append: incompatible types in variant (existing: {}, new: {})",
                    typeid(ExistingType).name(),
                    typeid(T).name());
            }
        },
            m_variant);

        // Update dimension size
        if (!m_dimensions.empty()) {
            m_dimensions[0].size = std::visit(
                [](const auto& vec) { return static_cast<uint64_t>(vec.size()); },
                m_variant);
        }
    }

    /**
     * @brief Append structured data to existing storage
     */
    template <GlmType T>
    void append_structured_to_existing(std::vector<T> new_data)
    {
        if (!std::holds_alternative<std::vector<T>>(m_variant)) {
            error<std::invalid_argument>(
                Journal::Component::Kakshya,
                Journal::Context::Runtime,
                std::source_location::current(),
                "Cannot append: existing variant doesn't hold matching GLM type ({})",
                typeid(T).name());
        }

        auto& existing = std::get<std::vector<T>>(m_variant);
        existing.insert(existing.end(), new_data.begin(), new_data.end());

        // Update dimension size
        if (!m_dimensions.empty()) {
            m_dimensions[0].size = existing.size();
        }
    }

    /**
     * @brief Convert modality to appropriate dimension name
     */
    static std::string modality_to_dimension_name(DataModality modality)
    {
        switch (modality) {
        case DataModality::VERTEX_POSITIONS_3D:
            return "positions";
        case DataModality::VERTEX_NORMALS_3D:
            return "normals";
        case DataModality::VERTEX_TANGENTS_3D:
            return "tangents";
        case DataModality::VERTEX_COLORS_RGB:
        case DataModality::VERTEX_COLORS_RGBA:
            return "colors";
        case DataModality::TEXTURE_COORDS_2D:
            return "texcoords";
        case DataModality::TRANSFORMATION_MATRIX:
            return "transforms";
        case DataModality::AUDIO_1D:
            return "samples";
        case DataModality::AUDIO_MULTICHANNEL:
            return "channels";
        default:
            return "data";
        }
    }

    /**
     * @brief Convert modality to dimension role
     */
    static DataDimension::Role modality_to_role(DataModality modality)
    {
        switch (modality) {
        case DataModality::AUDIO_1D:
        case DataModality::AUDIO_MULTICHANNEL:
            return DataDimension::Role::TIME;

        case DataModality::VERTEX_POSITIONS_3D:
        case DataModality::VERTEX_NORMALS_3D:
        case DataModality::VERTEX_TANGENTS_3D:
        case DataModality::TEXTURE_COORDS_2D:
            return DataDimension::Role::UV;

        case DataModality::VERTEX_COLORS_RGB:
        case DataModality::VERTEX_COLORS_RGBA:
            return DataDimension::Role::CHANNEL;

        default:
            return DataDimension::Role::CUSTOM;
        }
    }

    /**
     * @brief Suggest appropriate GLM type for a modality
     */
    static std::string_view suggest_glm_type_for_modality(DataModality modality)
    {
        switch (modality) {
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

        default:
            return "unknown";
        }
    }
};

} // namespace MayaFlux::Kakshya
