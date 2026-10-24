#pragma once

#include "GlyphAtlas.hpp"

namespace MayaFlux::Core {
class VKImage;
}

namespace MayaFlux::Portal::Text {

/**
 * @class AtlasImage
 * @brief The GPU image of a GlyphAtlas, for anything that samples glyph coverage.
 *
 * Holds an R8 image of the atlas and keeps it current: sync() uploads the
 * atlas when glyphs were rasterized since the last call, and recreates the
 * image when the atlas was replaced or grew. Growth rewrites every glyph's
 * UVs, so generation() increments whenever the image object changes; vertices
 * built against an earlier generation are stale and must be re-emitted, and
 * the image must be bound again where it was bound.
 *
 * The atlas dirty flag is consumed here: one AtlasImage per atlas.
 */
class MAYAFLUX_API AtlasImage {
public:
    /**
     * @brief Bring the image up to date with @p atlas.
     *
     * Creates the image on first use and again when @p atlas is a different
     * atlas, has a different size from the one last synced, or the image was
     * released by the texture manager shutting down. Otherwise
     * uploads the atlas pixels only when it is dirty.
     *
     * @param atlas Atlas to mirror.
     * @return The current image, or null if the texture manager is not
     *         initialized or the image could not be created.
     */
    [[nodiscard]] std::shared_ptr<Core::VKImage> sync(GlyphAtlas& atlas);

    /// @brief Increments every time sync() replaces the image object.
    [[nodiscard]] uint64_t generation() const noexcept { return m_generation; }

    /// @brief The image from the last sync(), null before the first.
    [[nodiscard]] const std::shared_ptr<Core::VKImage>& image() const noexcept { return m_image; }

private:
    std::shared_ptr<Core::VKImage> m_image;
    const GlyphAtlas* m_atlas { nullptr };
    uint32_t m_size { 0 };
    uint64_t m_generation { 0 };
};

} // namespace MayaFlux::Portal::Text
