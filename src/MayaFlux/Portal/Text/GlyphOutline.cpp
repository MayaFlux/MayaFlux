#include "GlyphOutline.hpp"

#include "MayaFlux/Portal/Text/GlyphAtlas.hpp"
#include "TypeFaceFoundry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

namespace MayaFlux::Portal::Text {

namespace {

    constexpr float k_ft_scale = 1.0F / 64.0F; ///< 26.6 fixed-point to pixels

    struct DecomposeCtx {
        std::vector<glm::vec2>* points;
        std::vector<uint32_t>* contour_ends;
        float tolerance;
        glm::vec2 current;
    };

    // ---------------------------------------------------------------------------
    // Recursive de Casteljau subdivision - appends points to ctx, excluding start
    // ---------------------------------------------------------------------------

    void subdivide_conic(
        DecomposeCtx& ctx,
        glm::vec2 p0,
        glm::vec2 p1,
        glm::vec2 p2,
        float tol_sq)
    {
        const glm::vec2 m01 = (p0 + p1) * 0.5F;
        const glm::vec2 m12 = (p1 + p2) * 0.5F;
        const glm::vec2 mid = (m01 + m12) * 0.5F;

        const glm::vec2 d = p2 - p0;
        const glm::vec2 perp { -d.y, d.x };
        const float dist_sq = (mid.x - p0.x) * perp.x + (mid.y - p0.y) * perp.y;

        if (dist_sq * dist_sq <= tol_sq * (perp.x * perp.x + perp.y * perp.y)) {
            ctx.points->push_back(p2);
            ctx.current = p2;
            return;
        }

        subdivide_conic(ctx, p0, m01, mid, tol_sq);
        subdivide_conic(ctx, mid, m12, p2, tol_sq);
    }

    void subdivide_cubic(
        DecomposeCtx& ctx,
        glm::vec2 p0,
        glm::vec2 p1,
        glm::vec2 p2,
        glm::vec2 p3,
        float tol_sq)
    {
        const glm::vec2 m01 = (p0 + p1) * 0.5F;
        const glm::vec2 m12 = (p1 + p2) * 0.5F;
        const glm::vec2 m23 = (p2 + p3) * 0.5F;
        const glm::vec2 m012 = (m01 + m12) * 0.5F;
        const glm::vec2 m123 = (m12 + m23) * 0.5F;
        const glm::vec2 mid = (m012 + m123) * 0.5F;

        const glm::vec2 d = p3 - p0;
        const glm::vec2 perp { -d.y, d.x };
        const float denom = perp.x * perp.x + perp.y * perp.y;

        auto dist_sq = [&](glm::vec2 p) {
            const float v = (p.x - p0.x) * perp.x + (p.y - p0.y) * perp.y;
            return v * v;
        };

        if (dist_sq(p1) <= tol_sq * denom && dist_sq(p2) <= tol_sq * denom) {
            ctx.points->push_back(p3);
            ctx.current = p3;
            return;
        }

        subdivide_cubic(ctx, p0, m01, m012, mid, tol_sq);
        subdivide_cubic(ctx, mid, m123, m23, p3, tol_sq);
    }

    // ---------------------------------------------------------------------------
    // FT_Outline_Decompose callbacks
    // ---------------------------------------------------------------------------

    int cb_move_to(const FT_Vector* to, void* user)
    {
        auto* ctx = static_cast<DecomposeCtx*>(user);
        if (!ctx->points->empty())
            ctx->contour_ends->push_back(static_cast<uint32_t>(ctx->points->size()));

        ctx->current = {
            static_cast<float>(to->x) * k_ft_scale,
            -static_cast<float>(to->y) * k_ft_scale
        };
        ctx->points->push_back(ctx->current);
        return 0;
    }

    int cb_line_to(const FT_Vector* to, void* user)
    {
        auto* ctx = static_cast<DecomposeCtx*>(user);
        ctx->current = {
            static_cast<float>(to->x) * k_ft_scale,
            -static_cast<float>(to->y) * k_ft_scale
        };
        ctx->points->push_back(ctx->current);
        return 0;
    }

    int cb_conic_to(const FT_Vector* ctrl, const FT_Vector* to, void* user)
    {
        auto* ctx = static_cast<DecomposeCtx*>(user);
        const glm::vec2 p1 {
            static_cast<float>(ctrl->x) * k_ft_scale,
            -static_cast<float>(ctrl->y) * k_ft_scale
        };
        const glm::vec2 p2 {
            static_cast<float>(to->x) * k_ft_scale,
            -static_cast<float>(to->y) * k_ft_scale
        };
        const float tol_sq = ctx->tolerance * ctx->tolerance;
        subdivide_conic(*ctx, ctx->current, p1, p2, tol_sq);
        return 0;
    }

    int cb_cubic_to(
        const FT_Vector* ctrl1,
        const FT_Vector* ctrl2,
        const FT_Vector* to,
        void* user)
    {
        auto* ctx = static_cast<DecomposeCtx*>(user);
        const glm::vec2 p1 {
            static_cast<float>(ctrl1->x) * k_ft_scale,
            -static_cast<float>(ctrl1->y) * k_ft_scale
        };
        const glm::vec2 p2 {
            static_cast<float>(ctrl2->x) * k_ft_scale,
            -static_cast<float>(ctrl2->y) * k_ft_scale
        };
        const glm::vec2 p3 {
            static_cast<float>(to->x) * k_ft_scale,
            -static_cast<float>(to->y) * k_ft_scale
        };
        const float tol_sq = ctx->tolerance * ctx->tolerance;
        subdivide_cubic(*ctx, ctx->current, p1, p2, p3, tol_sq);
        return 0;
    }

    constexpr FT_Outline_Funcs k_funcs {
        .move_to = cb_move_to,
        .line_to = cb_line_to,
        .conic_to = cb_conic_to,
        .cubic_to = cb_cubic_to,
        .shift = 0,
        .delta = 0,
    };

} // namespace

GlyphOutline decompose_glyph(
    FontFace& face,
    uint32_t codepoint,
    uint32_t pixel_size,
    float tolerance)
{
    GlyphOutline result;
    result.codepoint = codepoint;

    if (!face.is_loaded()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: FontFace not loaded");
        return result;
    }

    FT_Face ft = face.get_face();

    if (const FT_Error err = FT_Set_Pixel_Sizes(ft, 0, pixel_size); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: FT_Set_Pixel_Sizes({}) failed: {}", pixel_size, static_cast<int>(err));
        return result;
    }

    const FT_UInt idx = FT_Get_Char_Index(ft, static_cast<FT_ULong>(codepoint));
    if (idx == 0) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: no glyph for U+{:04X}", codepoint);
        return result;
    }

    if (const FT_Error err = FT_Load_Glyph(ft, idx, FT_LOAD_NO_BITMAP); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: FT_Load_Glyph({}) failed: {}", idx, static_cast<int>(err));
        return result;
    }

    result.advance_x = static_cast<int32_t>(ft->glyph->advance.x >> 6);

    if (ft->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
        return result;
    }

    DecomposeCtx ctx {
        .points = &result.points,
        .contour_ends = &result.contour_ends,
        .tolerance = tolerance,
        .current = {}
    };

    if (const FT_Error err = FT_Outline_Decompose(&ft->glyph->outline, &k_funcs, &ctx); err != 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: FT_Outline_Decompose failed: {}", static_cast<int>(err));
        result.points.clear();
        result.contour_ends.clear();
        return result;
    }

    if (!result.points.empty())
        result.contour_ends.push_back(static_cast<uint32_t>(result.points.size()));

    return result;
}

GlyphOutline decompose_glyph(uint32_t codepoint, float tolerance)
{
    auto* atlas = TypeFaceFoundry::instance().get_default_glyph_atlas();
    if (!atlas) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: no default font set");
        return GlyphOutline { .codepoint = codepoint };
    }

    auto& foundry = TypeFaceFoundry::instance();

    FontFace* face = foundry.get_default_face();
    if (!face) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "decompose_glyph: default face is null");
        return GlyphOutline { .codepoint = codepoint };
    }

    return decompose_glyph(*face, codepoint, atlas->pixel_size(), tolerance);
}

} // namespace MayaFlux::Portal::Text
