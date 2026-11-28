#ifndef MF_LIGHTING_INFLUENCE_GLSL
#define MF_LIGHTING_INFLUENCE_GLSL

/**
 * The influences of Nexus as lights, for the library's lit shaders. The surface
 * is set per renderer with specialization constants.
 */

#include "lighting.glsl"
#include "radiometry_tone.glsl"

layout(constant_id = 9) const float SURFACE_ROUGHNESS = 0.5;
layout(constant_id = 10) const float SURFACE_METALLIC = 0.0;
layout(constant_id = 11) const float SURFACE_DIFFUSE_ROUGHNESS = 0.0;
/** Whether colour textures hold sRGB values that were not decoded when sampled. */
layout(constant_id = 12) const bool SURFACE_TEXTURE_ENCODED = true;

/** One influence, as Nexus::InfluenceBlock mirrors it. */
struct InfluenceRecord {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
    uint shape;
    vec2 extent;
    vec3 direction;
    float cone_inner;
    vec3 attenuation;
    float cone_outer;
    vec3 right;
};

#ifdef MF_INFLUENCE_LIST
/** The buffer Nexus::InfluenceList fills. A place nothing holds is all zero. */
layout(set = 1, binding = 0, std430) readonly buffer Influences {
    uint influence_count;
    uint influence_unused0;
    uint influence_unused1;
    uint influence_unused2;
    InfluenceRecord influences[];
};

#define SURROUND_WIDTH 128
#define SURROUND_HEIGHT 64
#define SURROUND_LEVELS 6
#define SURROUND_TABLE 64
#define SURROUND_LOBES 32

/** What Nexus::InfluenceList::set_surround and set_lobes write. All zero is neither. */
layout(set = 1, binding = 1, std430) readonly buffer Surround {
    uint surround_present;
    uint lobes_present;
    uint surround_unused1;
    uint surround_unused2;
    vec4 surround_harmonics[9];
    vec4 surround_table[SURROUND_TABLE * SURROUND_TABLE];
    vec4 surround_lobes[SURROUND_LOBES * SURROUND_LOBES];
    vec4 surround_texels[];
};
#else
/** The block Nexus::InfluenceBlock mirrors. */
layout(push_constant) uniform Influence {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
    uint shape;
    vec2 extent;
    vec3 direction;
    float cone_inner;
    vec3 attenuation;
    float cone_outer;
    vec3 right;
};
#endif

/**
 * The light an influence stands for, by its Nexus::SourceShape. A point shape
 * is a spot where cone_outer is above zero. Cone angles are radians from the axis.
 */
Light influence_as_light(InfluenceRecord held)
{
    Light light = Light(held.position, held.intensity, held.color, held.radius,
        held.attenuation, vec3(0.0, 0.0, -1.0), 1.0, -1.0, LIGHT_POINT, vec3(1.0, 0.0, 0.0), held.extent);
    if (held.shape > 0u || held.cone_outer > 0.0) {
        light.direction = normalize(held.direction);
        light.right = normalize(held.right);
    }
    if (held.shape == 1u) {
        light.kind = LIGHT_DIRECTIONAL;
    } else if (held.shape > 1u) {
        light.kind = held.shape + 1u;
    } else if (held.cone_outer > 0.0) {
        light.kind = LIGHT_SPOT;
        light.cos_inner = cos(min(held.cone_inner, held.cone_outer));
        light.cos_outer = cos(held.cone_outer);
    }
    return light;
}

/** A colour texture sample as linear colour. */
vec3 surface_texture_color(vec3 sampled)
{
    return SURFACE_TEXTURE_ENCODED ? srgb_to_linear(sampled) : sampled;
}

#ifdef MF_INFLUENCE_LIST
vec3 surround_irradiance(vec3 n)
{
    float basis[9] = float[9](
        0.282095, 0.488603 * n.y, 0.488603 * n.z, 0.488603 * n.x,
        1.092548 * n.x * n.y, 1.092548 * n.y * n.z, 0.315392 * (3.0 * n.z * n.z - 1.0),
        1.092548 * n.x * n.z, 0.546274 * (n.x * n.x - n.y * n.y));
    vec3 sum = surround_harmonics[0].rgb * (MF_PI * basis[0]);
    for (int i = 1; i < 4; ++i) {
        sum += surround_harmonics[i].rgb * (2.0 * MF_PI / 3.0 * basis[i]);
    }
    for (int i = 4; i < 9; ++i) {
        sum += surround_harmonics[i].rgb * (MF_PI / 4.0 * basis[i]);
    }
    return max(sum, vec3(0.0));
}

vec3 surround_level(int level, vec3 direction)
{
    int width = SURROUND_WIDTH >> level;
    int height = SURROUND_HEIGHT >> level;
    int base = 0;
    for (int i = 0; i < level; ++i) {
        base += (SURROUND_WIDTH >> i) * (SURROUND_HEIGHT >> i);
    }

    float x = (atan(direction.z, direction.x) * 0.5 / MF_PI + 0.5) * float(width) - 0.5;
    float y = clamp(acos(clamp(direction.y, -1.0, 1.0)) / MF_PI * float(height) - 0.5, 0.0, float(height - 1));
    int x0 = (int(floor(x)) + width) % width;
    int x1 = (x0 + 1) % width;
    int y0 = int(floor(y));
    int y1 = min(y0 + 1, height - 1);
    vec2 f = vec2(fract(x), fract(y));
    return mix(
        mix(surround_texels[base + y0 * width + x0].rgb, surround_texels[base + y0 * width + x1].rgb, f.x),
        mix(surround_texels[base + y1 * width + x0].rgb, surround_texels[base + y1 * width + x1].rgb, f.x),
        f.y);
}

vec3 surround_radiance(vec3 direction, float roughness)
{
    float level = clamp(roughness, 0.0, 1.0) * float(SURROUND_LEVELS - 1);
    int low = int(level);
    int high = min(low + 1, SURROUND_LEVELS - 1);
    return mix(surround_level(low, direction), surround_level(high, direction), level - float(low));
}

/** Specular albedo scale and bias for a view cosine and roughness. */
vec2 surround_albedo(float cos_view, float roughness)
{
    vec2 at = clamp(vec2(cos_view, roughness) * float(SURROUND_TABLE) - 0.5, vec2(0.0), vec2(SURROUND_TABLE - 1));
    ivec2 low = ivec2(at);
    ivec2 high = min(low + 1, ivec2(SURROUND_TABLE - 1));
    vec2 f = at - vec2(low);
    return mix(
        mix(surround_table[low.y * SURROUND_TABLE + low.x].xy, surround_table[low.y * SURROUND_TABLE + high.x].xy, f.x),
        mix(surround_table[high.y * SURROUND_TABLE + low.x].xy, surround_table[high.y * SURROUND_TABLE + high.x].xy, f.x),
        f.y);
}

/** The fitted lobe for a view cosine and roughness. */
mat3 surround_lobe(float cos_view, float roughness)
{
    vec2 at = clamp(vec2(sqrt(max(1.0 - cos_view, 0.0)), roughness), vec2(0.0), vec2(1.0))
        * float(SURROUND_LOBES - 1);
    ivec2 low = ivec2(at);
    ivec2 high = min(low + 1, ivec2(SURROUND_LOBES - 1));
    vec2 f = at - vec2(low);
    vec4 t = mix(
        mix(surround_lobes[low.y * SURROUND_LOBES + low.x], surround_lobes[low.y * SURROUND_LOBES + high.x], f.x),
        mix(surround_lobes[high.y * SURROUND_LOBES + low.x], surround_lobes[high.y * SURROUND_LOBES + high.x], f.x),
        f.y);
    return mat3(vec3(t.x, 0.0, t.y), vec3(0.0, 1.0, 0.0), vec3(t.z, 0.0, t.w));
}

/** What the surroundings add. A zero normal receives the mean radiance diffusely. */
vec3 surround_contribution(SurfaceResponse surface, vec3 normal, vec3 view)
{
    float normal_sq = dot(normal, normal);
    if (!(normal_sq > 1e-8)) {
        return surface.base_color * max(surround_harmonics[0].rgb * 0.282095, vec3(0.0));
    }
    vec3 unit = normal * inversesqrt(normal_sq);
    vec2 albedo = surround_albedo(max(dot(unit, view), 0.0), surface.roughness);
    return environment_contribution(surface, unit, view, surround_irradiance(unit),
        surround_radiance(reflect(-view, unit), surface.roughness), albedo, 1.0);
}
#endif

/**
 * Displayable linear colour of a surface point lit by the influences. eye is
 * what view_eye() gives. A zero normal marks geometry without a surface
 * direction.
 */
vec3 influence_shade(vec3 base_color, vec3 at, vec3 normal, vec4 eye)
{
    SurfaceResponse surface = SurfaceResponse(
        base_color, SURFACE_ROUGHNESS, SURFACE_METALLIC, SURFACE_DIFFUSE_ROUGHNESS, 0.04);
    vec3 view = normalize(eye.xyz - at * eye.w);
    vec3 total = vec3(0.0);

#ifdef MF_INFLUENCE_LIST
    float specular_albedo = 1.0;
    LobeFit fit = LobeFit(false, mat3(1.0), vec2(0.0));
    float normal_sq = dot(normal, normal);
    if ((surround_present != 0u || lobes_present != 0u) && normal_sq > 1e-8) {
        float cos_view = max(dot(normal * inversesqrt(normal_sq), view), 0.0);
        vec2 albedo = surround_albedo(cos_view, surface.roughness);
        specular_albedo = albedo.x + albedo.y;
        if (lobes_present != 0u) {
            fit = LobeFit(true, surround_lobe(cos_view, surface.roughness), albedo);
        }
    }
    if (surround_present != 0u) {
        total += surround_contribution(surface, normal, view);
    }
    for (uint i = 0u; i < influence_count; ++i) {
        total += light_contribution(influence_as_light(influences[i]), surface, at, normal, view, specular_albedo, fit);
    }
#else
    InfluenceRecord held = InfluenceRecord(
        position, intensity, color, radius, size, shape, extent, direction, cone_inner, attenuation, cone_outer, right);
    total = light_contribution(influence_as_light(held), surface, at, normal, view, 1.0,
        LobeFit(false, mat3(1.0), vec2(0.0)));
#endif

    return pbr_neutral(max(total, vec3(0.0)));
}

#endif
