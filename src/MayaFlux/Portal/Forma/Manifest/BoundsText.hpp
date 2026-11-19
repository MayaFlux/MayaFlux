#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Transitive/Reflect/TextCodec.hpp"

namespace MayaFlux::Reflect {

/**
 * @brief A Kinesis::AABB2D as four finite numbers in the struct's own member order.
 *
 * Reads min.x min.y max.x max.y, so the text is the aggregate
 * AABB2D { .min = { x0, y0 }, .max = { x1, y1 } } written flat. An origin and size
 * is a different spelling and has its own named constructor, AABB2D::from_origin_size.
 */
template <>
struct TextCodec<Kinesis::AABB2D> {
    static constexpr size_t extent = 4;

    static bool decode(std::string_view source, Kinesis::AABB2D& value, TextError& error)
    {
        const auto decoded = parse_tuple<float, 4>(source, error);
        if (!decoded)
            return false;
        value.min = { (*decoded)[0], (*decoded)[1] };
        value.max = { (*decoded)[2], (*decoded)[3] };
        return true;
    }
};

}
