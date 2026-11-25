#pragma once

/**
 * @file AlembicShared.hpp
 * @brief What the Alembic reader and writers agree on. Include from implementation files only.
 */

#include "MayaFlux/Kakshya/NDData/SpatialData.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include <windows.h>
#endif

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcMaterial/All.h>

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#ifdef ERROR
#undef ERROR
#endif
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#endif // MAYAFLUX_PLATFORM_WINDOWS

namespace MayaFlux::IO::alembic {

    using namespace Alembic::AbcGeom;

    inline constexpr const char* k_material_target = "mayaflux";
    inline constexpr const char* k_surface_type = "surface";
    inline constexpr const char* k_properties_type = "properties";
    inline constexpr const char* k_private_prefix = "mayaflux_";
    inline constexpr const char* k_weight_param = "mayaflux_weight";
    inline constexpr const char* k_orthographic_property = "mayaflux_orthographic_width";

    inline constexpr std::array<const char*, 7> k_map_slots {
        "baseColor", "normal", "metalness", "roughness", "emissive", "occlusion", "opacity"
    };

    inline GeometryScope to_alembic_scope(Kakshya::SpatialScope scope)
    {
        switch (scope) {
        case Kakshya::SpatialScope::Constant:
            return kConstantScope;
        case Kakshya::SpatialScope::Uniform:
            return kUniformScope;
        case Kakshya::SpatialScope::Varying:
            return kVaryingScope;
        }
        return kConstantScope;
    }

    /** @brief A float GeomParam declared with array extent 4, kept apart from a scalar float param. */
    struct OFloat4GeomParam {
        OFloatGeomParam param;
    };

    using GeomParamVariant = std::variant<
        OFloatGeomParam, OV2fGeomParam, OV3fGeomParam, OC3fGeomParam, ON3fGeomParam, OUInt32GeomParam,
        OC4fGeomParam, OFloat4GeomParam>;

    /**
     * @brief Find or declare @p attr's OGeomParam, then set this sample's values.
     *
     * A vec3 colour, or a vec3 attribute named "normal", is declared through
     * Alembic's dedicated OC3fGeomParam/ON3fGeomParam rather than a plain
     * OV3fGeomParam: those are MayaFlux's own fixed vertex field names
     * (PointVertex/LineVertex/MeshVertex all use them for these exact
     * roles), and the dedicated type, not the value's shape (three floats
     * either way), is what tells an importer this is actually color or
     * normal data rather than an arbitrary vector. Any other vec3 name
     * (e.g. "tangent") stays a generic vector, matching what Alembic
     * itself offers no dedicated type for.
     *
     * A vec4 colour is declared as OC4fGeomParam (RGBA). Any other vec4
     * is a float GeomParam with array extent 4.
     *
     * @return False if @p values holds a DataVariant alternative with no
     *         Alembic GeomParam counterpart, its size doesn't match
     *         @p expected_count, or @p name was already declared with a
     *         different type.
     */
    inline bool set_attribute(
        std::unordered_map<std::string, GeomParamVariant>& params,
        const OCompoundProperty& arb_params,
        const std::string& name,
        const Kakshya::DataVariant& values,
        GeometryScope scope,
        size_t expected_count,
        bool as_color)
    {
        auto it = params.find(name);

        if (const auto* floats = std::get_if<std::vector<float>>(&values)) {
            if (floats->size() != expected_count) {
                return false;
            }
            if (it == params.end()) {
                it = params.emplace(name, OFloatGeomParam(arb_params, name, false, scope, 1)).first;
            }
            auto* param = std::get_if<OFloatGeomParam>(&it->second);
            if (!param) {
                return false;
            }
            param->set(OFloatGeomParam::Sample(FloatArraySample(floats->data(), floats->size()), scope));
            return true;
        }
        if (const auto* uvs = std::get_if<std::vector<glm::vec2>>(&values)) {
            if (uvs->size() != expected_count) {
                return false;
            }
            if (it == params.end()) {
                it = params.emplace(name, OV2fGeomParam(arb_params, name, false, scope, 1)).first;
            }
            auto* param = std::get_if<OV2fGeomParam>(&it->second);
            if (!param) {
                return false;
            }
            param->set(OV2fGeomParam::Sample(
                V2fArraySample(reinterpret_cast<const V2f*>(uvs->data()), uvs->size()), scope));
            return true;
        }
        if (const auto* vecs = std::get_if<std::vector<glm::vec3>>(&values)) {
            if (vecs->size() != expected_count) {
                return false;
            }

            if (it == params.end()) {
                if (as_color) {
                    it = params.emplace(name, OC3fGeomParam(arb_params, name, false, scope, 1)).first;
                } else if (name == "normal") {
                    it = params.emplace(name, ON3fGeomParam(arb_params, name, false, scope, 1)).first;
                } else {
                    it = params.emplace(name, OV3fGeomParam(arb_params, name, false, scope, 1)).first;
                }
            }

            if (auto* color_param = std::get_if<OC3fGeomParam>(&it->second)) {
                color_param->set(OC3fGeomParam::Sample(
                    C3fArraySample(reinterpret_cast<const C3f*>(vecs->data()), vecs->size()), scope));
                return true;
            }
            if (auto* normal_param = std::get_if<ON3fGeomParam>(&it->second)) {
                normal_param->set(ON3fGeomParam::Sample(
                    N3fArraySample(reinterpret_cast<const N3f*>(vecs->data()), vecs->size()), scope));
                return true;
            }
            if (auto* vector_param = std::get_if<OV3fGeomParam>(&it->second)) {
                vector_param->set(OV3fGeomParam::Sample(
                    V3fArraySample(reinterpret_cast<const V3f*>(vecs->data()), vecs->size()), scope));
                return true;
            }
            return false;
        }
        if (const auto* quads = std::get_if<std::vector<glm::vec4>>(&values)) {
            if (quads->size() != expected_count) {
                return false;
            }

            if (it == params.end()) {
                if (as_color) {
                    it = params.emplace(name, OC4fGeomParam(arb_params, name, false, scope, 1)).first;
                } else {
                    it = params.emplace(name,
                                   OFloat4GeomParam { OFloatGeomParam(arb_params, name, false, scope, 4) })
                             .first;
                }
            }

            if (auto* color_param = std::get_if<OC4fGeomParam>(&it->second)) {
                color_param->set(OC4fGeomParam::Sample(
                    C4fArraySample(reinterpret_cast<const C4f*>(quads->data()), quads->size()), scope));
                return true;
            }
            if (auto* vector_param = std::get_if<OFloat4GeomParam>(&it->second)) {
                vector_param->param.set(OFloatGeomParam::Sample(
                    FloatArraySample(reinterpret_cast<const float*>(quads->data()), quads->size() * 4), scope));
                return true;
            }
            return false;
        }
        if (const auto* uints = std::get_if<std::vector<uint32_t>>(&values)) {
            if (uints->size() != expected_count) {
                return false;
            }
            if (it == params.end()) {
                it = params.emplace(name, OUInt32GeomParam(arb_params, name, false, scope, 1)).first;
            }
            auto* param = std::get_if<OUInt32GeomParam>(&it->second);
            if (!param) {
                return false;
            }
            param->set(OUInt32GeomParam::Sample(UInt32ArraySample(uints->data(), uints->size()), scope));
            return true;
        }
        return false;
    }

} // namespace MayaFlux::IO::alembic
