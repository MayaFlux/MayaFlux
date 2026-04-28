#pragma once

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Portal/Forma/Element.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Portal::Forma {

/**
 * @brief Version counter incremented on every value write.
 *
 * Plain uint64_t behind a shared_ptr. The graphics tick compares against
 * its last-seen version to detect changes without polling the value itself.
 * Not atomic — same scheduler tick, no concurrent access.
 */
using Version = uint64_t;

/**
 * @struct MappedState
 * @brief Value carrier for a Mapped primitive.
 *
 * Owns the current value and a version counter. Any writer increments
 * version after updating value. The graphics tick reads both and decides
 * whether to regenerate geometry.
 *
 * Exposed as shared_ptr<MappedState<T>> so any external system — a node
 * output reader, a buffer writer, a scheduler callback — can hold a
 * reference and read or write without knowing about the primitive.
 *
 * @tparam T Value type. float for a single continuous parameter,
 *           glm::vec2 for a 2D position, any plain data type.
 */
template <typename T>
struct MappedState {
    T value {};
    Version version { 0 };
    uint32_t id { 0 };

    void write(T v)
    {
        value = v;
        ++version;
    }
};

/**
 * @brief Geometry function signature.
 *
 * Called by the graphics tick when the version has advanced. Receives the
 * current value and an output byte vector to fill with interleaved vertex
 * data. Layout, stride, and topology are determined by how the caller
 * constructed the VKBuffer and RenderProcessor — the function is agnostic
 * to all of those.
 *
 * The function is also responsible for updating the Element's spatial
 * description if the geometry change affects hit testing (e.g. a handle
 * that moves). It receives the Element by reference for that purpose.
 *
 * @tparam T Value type matching the MappedState.
 */
template <typename T>
using GeometryFn = std::function<void(T value, std::vector<uint8_t>& out_bytes, Element& element)>;

/**
 * @struct Mapped
 * @brief Infrastructure for a continuously-mapped value whose GPU geometry
 *        tracks it.
 *
 * The geometry function IS the primitive. Mapped provides only the
 * mechanism: a shared value carrier, a version counter, and a sync()
 * call that invokes the geometry function when the value changes.
 *
 * The geometry function decides everything about shape, topology, and
 * vertex layout. It can write from any source — mouse pixel coordinates,
 * microphone energy, another node's output, a tendency field, a computed
 * function of time — by whatever means the caller chooses. The common
 * geometry helpers in Geometry.hpp are illustrative, not idiomatic.
 *
 * Orchestration (node binding, buffer mapping, scheduler-driven update,
 * input capture) attaches externally by writing to state and calling
 * state->write(v). Mapped has no knowledge of or dependency on any
 * orchestration layer.
 *
 * @tparam T Value type.
 */
template <typename T>
struct Mapped {
    /// @brief Shared value carrier. External systems hold a copy of this ptr.
    std::shared_ptr<MappedState<T>> state;

    /// @brief Geometry regeneration function. Set once at construction.
    GeometryFn<T> geometry_fn;

    /// @brief The Element registered with the Layer.
    ///        element.id is valid after Layer::add(element) has been called.
    Element element;

    /// @brief Last version seen by sync(). Initialised to a sentinel that
    ///        guarantees one geometry generation on the first sync() call.
    Version last_version { static_cast<Version>(-1) };

    /**
     * @brief Call once per graphics tick.
     *
     * If state->version has advanced since last_version, invokes geometry_fn
     * with the current value into a local byte buffer, then passes the result
     * to VKBuffer::set_data so the existing upload path handles GPU transfer.
     *
     * The caller is responsible for driving the graphics tick — Mapped does
     * not register itself with any scheduler.
     */
    void sync()
    {
        if (!state || !geometry_fn || !element.buffer)
            return;
        if (state->version == last_version)
            return;

        geometry_fn(state->value, m_bytes, element);
        element.buffer->submit(m_bytes);
        last_version = state->version;
    }

private:
    std::vector<uint8_t> m_bytes;
};

/**
 * @brief Construct a Mapped<T> with a freshly allocated MappedState.
 * @param initial   Starting value.
 * @param geometry  Geometry function producing vertex bytes from a value.
 * @param buffer    VKBuffer whose raw bytes the geometry function writes into.
 * @return Ready-to-use Mapped. Register element with a Layer before first sync().
 */
template <typename T>
[[nodiscard]] Mapped<T> make_mapped(
    T initial,
    GeometryFn<T> geometry,
    std::shared_ptr<Buffers::FormaBuffer> buffer)
{
    auto st = std::make_shared<MappedState<T>>();
    st->write(initial);

    Element el;
    el.buffer = std::move(buffer);

    Mapped<T> m;
    m.state = std::move(st);
    m.geometry_fn = std::move(geometry);
    m.element = std::move(el);
    m.last_version = static_cast<Version>(-1);
    return m;
}

} // namespace MayaFlux::Portal::Forma
