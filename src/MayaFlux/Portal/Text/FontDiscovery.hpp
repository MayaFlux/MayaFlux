#pragma once

namespace MayaFlux::Portal::Text {

/**
 * @brief Locate a system font file by family name and optional style.
 *
 * Queries the platform font subsystem without requiring any bundled assets.
 * On Linux: links against fontconfig and calls FcFontMatch.
 * On macOS: shells out to fc-match if fontconfig is available, otherwise
 *           walks ~/Library/Fonts and /System/Library/Fonts.
 * On Windows: walks %WINDIR%\Fonts for a filename containing @p family.
 *
 * The result is an absolute path suitable for passing directly to
 * Portal::Text::set_default_font() or FontFace::load().
 *
 * @param family  Font family name, e.g. "JetBrains Mono", "DejaVu Sans".
 * @param style   Optional style hint, e.g. "Medium", "Bold", "Italic".
 *                Passed to fontconfig on Linux; ignored on other platforms.
 * @return        Absolute path to a matching font file, or std::nullopt
 *                if no match could be found.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::string> find_font(
    std::string_view family,
    std::string_view style = {});

} // namespace MayaFlux::Portal::Text
