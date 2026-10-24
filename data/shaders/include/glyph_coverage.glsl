#ifndef GLYPH_COVERAGE_GLSL
#define GLYPH_COVERAGE_GLSL

/// @brief Whether a vertex weight marks a fragment as glyph coverage.
///
/// Positive weights select a texture slot as an image and zero is a flat
/// colour. A negative weight selects slot -weight - 1 as glyph coverage: the
/// slot holds the atlas, and only its red channel is read. Mirrors
/// Portal::Text::coverage_weight.
bool glyph_is_coverage(float weight)
{
    return weight < -0.5;
}

/// @brief Zero based texture slot a coverage weight selects.
///        Only meaningful when glyph_is_coverage(weight) is true.
int glyph_slot(float weight)
{
    return int(-weight + 0.5) - 1;
}

#endif
