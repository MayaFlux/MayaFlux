#pragma once

#include "MayaFlux/Transitive/Reflect/TextCodec.hpp"

namespace MayaFlux::Reflect {

/**
 * @brief TextCodec extension for GLM vectors.
 *
 * Decodes a vector from whitespace separated finite components, one per
 * component. Include this header before any binding that decodes GLM members;
 * the specialization is then found automatically.
 */
template <GlmVectorType T>
struct TextCodec<T> {
    static constexpr size_t extent = glm_component_count<T>();

    static bool decode(std::string_view source, T& value, TextError& error)
    {
        using Scalar = typename T::value_type;
        constexpr size_t count = glm_component_count<T>();
        const auto decoded = parse_tuple<Scalar, count>(source, error);
        if (!decoded)
            return false;
        for (size_t i = 0; i < count; ++i)
            value[static_cast<glm::length_t>(i)] = (*decoded)[i];
        return true;
    }
};

/**
 * @brief A sequence of GLM vectors: numbers are read in groups of the vector's component count.
 *
 * Reports Cardinality when the number of values is not a multiple of the
 * component count. An empty source gives an empty sequence.
 */
template <GlmVectorType T>
struct TextCodec<std::vector<T>> {
    static constexpr size_t extent = glm_component_count<T>();
    static constexpr bool sequence = true;

    static bool decode(std::string_view source, std::vector<T>& value, TextError& error)
    {
        using Scalar = typename T::value_type;
        constexpr size_t count = glm_component_count<T>();
        const auto numbers = parse_numbers<Scalar>(source);
        if (!numbers) {
            error = TextError::Invalid;
            return false;
        }
        if (numbers->size() % count != 0) {
            error = TextError::Cardinality;
            return false;
        }
        std::vector<T> decoded(numbers->size() / count);
        for (size_t i = 0; i < decoded.size(); ++i) {
            for (size_t j = 0; j < count; ++j)
                decoded[i][static_cast<glm::length_t>(j)] = (*numbers)[i * count + j];
        }
        value = std::move(decoded);
        return true;
    }
};

}
