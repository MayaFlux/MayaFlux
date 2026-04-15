#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

namespace MayaFlux::Portal::Text {

/**
 * @class FreeTypeContext
 * @brief Singleton owner of the FT_Library handle.
 *
 * Initialised once by Portal::Text::initialize() and shut down by
 * Portal::Text::shutdown().  All FontFace instances borrow the library
 * handle via get_library(); they must not outlive this singleton.
 */
class MAYAFLUX_API FreeTypeContext {
public:
    static FreeTypeContext& instance()
    {
        static FreeTypeContext ctx;
        return ctx;
    }

    FreeTypeContext(const FreeTypeContext&) = delete;
    FreeTypeContext& operator=(const FreeTypeContext&) = delete;
    FreeTypeContext(FreeTypeContext&&) = delete;
    FreeTypeContext& operator=(FreeTypeContext&&) = delete;

    /**
     * @brief Initialise the FreeType library.
     * @return true on success, false if FT_Init_FreeType fails.
     */
    bool initialize();

    /**
     * @brief Release the FreeType library handle.
     */
    void shutdown();

    /**
     * @brief Returns true after a successful initialize() call.
     */
    [[nodiscard]] bool is_initialized() const { return m_library != nullptr; }

    /**
     * @brief Raw FT_Library handle for use by FontFace.
     * @pre is_initialized() must be true.
     */
    [[nodiscard]] FT_Library get_library() const { return m_library; }

private:
    FreeTypeContext() = default;
    ~FreeTypeContext() { shutdown(); }

    FT_Library m_library { nullptr };
};

} // namespace MayaFlux::Portal::Text
