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

}
