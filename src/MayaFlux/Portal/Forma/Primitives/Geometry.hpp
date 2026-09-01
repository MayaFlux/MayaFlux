#pragma once

#include "Mapped.hpp"

namespace MayaFlux::Portal::Forma {
class Context;
}

namespace MayaFlux::Portal::Forma::Geometry {

/**
 * @file Geometry.hpp
 * @brief Illustrative geometry functions for common Mapped use cases.
 *
 * These are starting points for reading and understanding the GeometryFn
 * contract, not the primary or idiomatic way to use Mapped in MayaFlux.
 *
 * The idiomatic use is a custom geometry function that writes from whatever
 * data source makes sense: mouse pixel coordinates written directly as buffer
 * data, microphone energy mapped to a spatial form, a node output driving
 * vertex positions, a tendency field evaluated at runtime. The function
 * receives a value and an output byte buffer — what it does with those is
 * unconstrained.
 *
 * The helpers below demonstrate the pattern concretely. They are not
 * privileged and carry no special status in the framework.
 */

/**
 * @brief Write a vertex array into a GeometryFn output buffer.
 * @param out   Output buffer. Resized to fit @p verts exactly.
 * @param verts Vertices to copy.
 */
template <typename V>
void write_verts(std::vector<uint8_t>& out, const std::vector<V>& verts)
{
    out.resize(verts.size() * sizeof(V));
    std::memcpy(out.data(), verts.data(), out.size());
}

/**
 * @brief Write a contiguous range of trivially-copyable vertices into a GeometryFn output buffer.
 */
template <typename V>
    requires std::ranges::contiguous_range<V>
    && std::is_trivially_copyable_v<std::ranges::range_value_t<V>>
void write_verts(std::vector<uint8_t>& out, const V& verts)
{
    const size_t n = std::ranges::size(verts) * sizeof(std::ranges::range_value_t<V>);
    out.resize(n);
    std::memcpy(out.data(), std::ranges::data(verts), n);
}

template <typename V>
    requires std::is_trivially_copyable_v<V>
    && (!std::ranges::range<V>)
void write_verts(std::vector<uint8_t>& out, const V& v)
{
    out.resize(sizeof(v));
    std::memcpy(out.data(), &v, sizeof(v));
}

/**
 * @struct Form
 * @brief A geometry function plus everything its realization requires.
 *
 * A geometry function alone is not enough to construct an element. Its
 * topology is fixed by the vertex kind it writes, its capacity by the
 * vertex count, and its interaction by the forward placement it performs.
 * All three are knowledge the geometry factory already has and the call
 * site would otherwise restate. Form carries them together.
 *
 * Nothing enumerates the set of Forms. Any function returning one
 * participates on equal footing with those in FormFactory.hpp, including
 * ones written by the caller. There is no dispatch and no registry.
 *
 * A bare GeometryFn converts implicitly, yielding default topology,
 * default capacity, and no interaction.
 *
 * @tparam T MappedState value type driving the geometry.
 */
template <typename T>
struct Form {
    /// @brief Writes vertex bytes for the current value.
    GeometryFn<T> geometry;

    /// @brief Topology matching the vertex kind @c geometry writes.
    Graphics::PrimitiveTopology topology { Graphics::PrimitiveTopology::TRIANGLE_STRIP };

    /// @brief FormaBuffer capacity in bytes for the vertex count @c geometry writes.
    size_t capacity { 4096 };

    /**
     * @brief Registers interaction against the realized element.
     *
     * Called once after the element is registered, with the Context that
     * owns its callbacks, the element id, and the state to write. Empty
     * for non-interactive geometry.
     */
    std::function<void(Context&, uint32_t, std::shared_ptr<MappedState<T>>)> wire;

    Form() = default;

    /**
     * @brief Adopt a bare geometry function with default topology and
     *        capacity and no interaction.
     */
    Form(GeometryFn<T> fn)
        : geometry(std::move(fn))
    {
    }

    Form(GeometryFn<T> fn, Graphics::PrimitiveTopology topo, size_t cap,
        std::function<void(Context&, uint32_t, std::shared_ptr<MappedState<T>>)> w = {})
        : geometry(std::move(fn))
        , topology(topo)
        , capacity(cap)
        , wire(std::move(w))
    {
    }
};

/**
 * @brief Call a bounds-taking factory with an anchor's region.
 *
 * Any factory whose first parameter is a Kinesis::AABB2D accepts any
 * Kinesis::HasBounds object through this, including factories written by
 * the caller. Nothing is enumerated and no geometry is privileged.
 *
 * Trailing factory defaults still apply; arguments passed here are
 * forwarded in order after the derived bounds.
 *
 * @code
 * auto fader = Forma::create(surface, Geometry::horizontal_fader(track, 0.04F), 0.5F);
 * auto meter = Forma::create(surface, Geometry::at(fader, Geometry::level_meter), 0.F);
 * @endcode
 *
 * @param anchor  Any object exposing bounds().
 * @param factory Factory taking Kinesis::AABB2D as its first parameter.
 * @param args    Remaining factory arguments.
 */
template <Kinesis::HasBounds B, typename F, typename... Args>
[[nodiscard]] auto at(const B& anchor, F&& factory, Args&&... args)
    -> decltype(std::forward<F>(factory)(Kinesis::bounds_of(anchor), std::forward<Args>(args)...))
{
    return std::forward<F>(factory)(Kinesis::bounds_of(anchor), std::forward<Args>(args)...);
}

} // namespace MayaFlux::Portal::Forma::Geometry
