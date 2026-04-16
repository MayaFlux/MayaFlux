#pragma once

#include "TextureBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class TextBuffer
 * @brief TextureBuffer specialization for CPU-composited glyph textures.
 *
 * Encodes two invariants that hold for all pre-multiplied RGBA glyph
 * coverage textures: streaming mode is enabled on the TextureProcessor
 * to avoid per-frame staging buffer allocation under animated text, and
 * alpha blending is enabled on the RenderProcessor so glyph coverage
 * composites correctly over whatever is behind it.
 *
 * Produced by Portal::Text::InkPress. Callers interact with it identically
 * to TextureBuffer -- set_position, set_scale, setup_rendering -- with no
 * knowledge of the invariants required.
 *
 * When constructed with a budget (via Portal::Text::press overload accepting
 * budget_width/budget_height), the GPU texture is allocated at the budget
 * dimensions and the unused region is zero-filled (fully transparent).
 * Subsequent Portal::Text::repress calls composite into the allocated region
 * without reallocating unless the new content exceeds the budget and the
 * policy is RedrawPolicy::Fit.
 *
 * Prefer pre-allocating a budget and using Portal::Text::impress for
 * incremental text updates. RedrawPolicy::Fit on every repress call
 * reallocates the VKImage and rebuilds the descriptor on each update.
 */
class MAYAFLUX_API TextBuffer : public TextureBuffer {
public:
    using TextureBuffer::TextureBuffer;

    /**
     * @brief Whether this buffer was created with a pre-allocated budget
     *        exceeding the initial content dimensions.
     */
    [[nodiscard]] bool has_budget() const { return m_budget_width > 0; }

    /**
     * @brief Full allocated GPU texture width. Equal to get_width() when
     *        no budget was set.
     */
    [[nodiscard]] uint32_t get_budget_width() const
    {
        return m_budget_width > 0 ? m_budget_width : get_width();
    }

    /**
     * @brief Full allocated GPU texture height. Equal to get_height() when
     *        no budget was set.
     */
    [[nodiscard]] uint32_t get_budget_height() const
    {
        return m_budget_height > 0 ? m_budget_height : get_height();
    }

    /**
     * @brief Sets the budget dimensions for this buffer. Does not allocate
     *        GPU texture or modify the buffer in any way; the budget is
     *        only used as a hint by Portal::Text::repress when deciding
     *        whether to reallocate.
     * @param w  Budget width in pixels.
     * @param h  Budget height in pixels.
     */
    void set_budget(uint32_t w, uint32_t h)
    {
        m_budget_width = w;
        m_budget_height = h;
    }

    /**
     * @brief Horizontal pen position in pixels for the next impress() run.
     *        Reset to zero by repress(). Advanced by impress().
     */
    [[nodiscard]] const uint32_t& get_cursor_x() const { return m_cursor_x; }
    [[nodiscard]] uint32_t& get_cursor_x() { return m_cursor_x; }

    /**
     * @brief Vertical baseline position in pixels for the next impress() run.
     *        Reset to zero by repress(). Advanced by impress() on line wrap.
     */
    [[nodiscard]] const uint32_t& get_cursor_y() const { return m_cursor_y; }
    [[nodiscard]] uint32_t& get_cursor_y() { return m_cursor_y; }

    /**
     * @brief Resets the write cursor to the origin. Called internally by
     *        repress(). Callers may also reset manually between logical blocks.
     */
    void reset_cursor()
    {
        m_cursor_x = 0;
        m_cursor_y = 0;
    }

    /**
     * @brief Hard render bounds. impress() wraps at render_bounds_w and
     *        returns Overflow when render_bounds_h is exhausted.
     */
    void set_render_bounds(uint32_t w, uint32_t h)
    {
        m_render_bounds_w = w;
        m_render_bounds_h = h;
    }

    [[nodiscard]] uint32_t get_render_bounds_w() const { return m_render_bounds_w; }
    [[nodiscard]] uint32_t get_render_bounds_h() const { return m_render_bounds_h; }

    /**
     * @brief Full accumulated text composited into this buffer since the
     *        last repress(). Seeded by press(), appended by impress(),
     *        cleared by repress().
     */
    [[nodiscard]] const std::string& get_accumulated_text() const { return m_accumulated_text; }
    void append_accumulated_text(std::string_view s) { m_accumulated_text += s; }
    void clear_accumulated_text() { m_accumulated_text.clear(); }
    void set_accumulated_text(std::string_view s) { m_accumulated_text = s; }

    /**
     * @brief Delegates to TextureBuffer::setup_rendering, then enables
     *        streaming mode on the TextureProcessor and alpha blending
     *        on the RenderProcessor.
     * @param config  Same as TextureBuffer::setup_rendering.
     */
    void setup_rendering(const RenderConfig& config) override;

private:
    /// @brief Allocated GPU texture width when a budget was requested.
    ///        Zero when the buffer was created without a budget.
    uint32_t m_budget_width {};
    /// @brief Allocated GPU texture height when a budget was requested.
    uint32_t m_budget_height {};

    uint32_t m_cursor_x {};
    uint32_t m_cursor_y {};

    uint32_t m_render_bounds_w { 1280 };
    uint32_t m_render_bounds_h { 720 };
    std::string m_accumulated_text;
};

} // namespace MayaFlux::Buffers
