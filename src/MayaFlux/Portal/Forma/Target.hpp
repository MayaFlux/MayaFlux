#pragma once

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @class FormaTarget
 * @brief Where an element's geometry and texture are written.
 *
 * A handle to a FormaBuffer, or to one slice of a FormaBuffer that several
 * elements share. Everything that writes or controls an element's output goes
 * through it, so the element does not need to know whether it owns a buffer or
 * a slice of one. A shared_ptr<FormaBuffer> converts to it implicitly and means
 * the whole buffer, so code that creates its own buffer is unchanged.
 *
 * Copies of one handle share their state: when a slice moves to another
 * buffer, every copy follows. operator-> reaches the underlying buffer for
 * buffer-level state such as the render processor or vertex layout; content
 * and visibility go through the methods below.
 */
class MAYAFLUX_API FormaTarget {
public:
    using SliceId = Buffers::FormaBuffer::SliceId;

    FormaTarget() = default;
    FormaTarget(std::nullptr_t) noexcept { }

    /**
     * @brief A new empty slice of @p buffer, drawn after the others.
     *
     * Hands several creators one buffer. Slices of a topology other than the
     * buffer's need a buffer set up to triangulate.
     *
     * @return An empty handle if the buffer is not ready for slices.
     */
    [[nodiscard]] static FormaTarget slice_of(
        const std::shared_ptr<Buffers::FormaBuffer>& buffer,
        std::optional<Graphics::PrimitiveTopology> topology = std::nullopt);

    /** @brief The whole buffer. */
    FormaTarget(std::shared_ptr<Buffers::FormaBuffer> buffer);

    /** @brief One slice of @p buffer, as returned by FormaBuffer::add_slice. */
    FormaTarget(std::shared_ptr<Buffers::FormaBuffer> buffer, SliceId slice);

    [[nodiscard]] explicit operator bool() const noexcept { return m_state != nullptr; }

    [[nodiscard]] Buffers::FormaBuffer* operator->() const noexcept;
    [[nodiscard]] Buffers::FormaBuffer& operator*() const noexcept;

    /** @brief The underlying buffer; null for an empty handle. */
    [[nodiscard]] const std::shared_ptr<Buffers::FormaBuffer>& buffer() const noexcept;

    /** @brief Slice id, 0 when the handle means the whole buffer. */
    [[nodiscard]] SliceId slice() const noexcept;

    /** @brief Replace this target's geometry. */
    void submit(const std::vector<uint8_t>& bytes) const;

    /** @brief Replace this target's geometry with a contiguous range of vertices. */
    template <typename V>
        requires std::ranges::contiguous_range<V>
        && std::is_trivially_copyable_v<std::ranges::range_value_t<V>>
    void submit(const V& vertices) const
    {
        const auto* src = reinterpret_cast<const uint8_t*>(std::ranges::data(vertices));
        const size_t n = std::ranges::size(vertices) * sizeof(std::ranges::range_value_t<V>);
        submit(std::vector<uint8_t>(src, src + n));
    }

    /** @brief Replace this target's geometry with a single vertex. */
    template <typename V>
        requires std::is_trivially_copyable_v<V>
        && (!std::ranges::range<V>)
    void submit(const V& vertex) const
    {
        const auto* src = reinterpret_cast<const uint8_t*>(&vertex);
        submit(std::vector<uint8_t>(src, src + sizeof(V)));
    }

    /** @brief Bind a texture in the buffer's texture array. */
    void bind_texture(uint32_t array_index, const std::shared_ptr<Core::VKImage>& image) const;

    /** @brief Show or hide this target's output. */
    void set_visible(bool visible) const;

    /**
     * @brief Draw this target after the others.
     * @param priority Draw priority given to a whole buffer. A slice keeps the
     *        priority of its buffer and moves to the end of the buffer's order.
     */
    void bring_to_front(int64_t priority) const;

    /** @brief Draw this target before the others. See bring_to_front. */
    void send_to_back(int64_t priority) const;

    /**
     * @brief Release this target: retire a whole buffer, or remove a slice.
     *
     * A buffer left with no slice is retired with the last one.
     */
    void retire() const;

    [[nodiscard]] friend bool operator==(const FormaTarget& a, const FormaTarget& b) noexcept
    {
        return a.buffer() == b.buffer() && a.slice() == b.slice();
    }

    [[nodiscard]] friend bool operator==(const FormaTarget& a, const std::shared_ptr<Buffers::FormaBuffer>& b) noexcept
    {
        return a.buffer() == b;
    }

    [[nodiscard]] friend bool operator==(const FormaTarget& a, std::nullptr_t) noexcept
    {
        return !a;
    }

private:
    struct State {
        std::shared_ptr<Buffers::FormaBuffer> buffer;
        SliceId slice {};
    };

    std::shared_ptr<State> m_state;
};

} // namespace MayaFlux::Portal::Forma
