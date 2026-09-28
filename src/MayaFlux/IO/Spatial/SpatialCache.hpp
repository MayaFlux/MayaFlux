#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Portal::Graphics {
enum class PrimitiveTopology : uint8_t;
}

namespace MayaFlux::IO {

/**
 * @enum SpatialScope
 * @brief How many elements one attribute contributes per sample.
 *
 * Mirrors Alembic's own GeometryScope, narrowed to the three values that
 * apply to points and curves (kVertexScope/kFacevaryingScope are mesh-only
 * concepts with no equivalent here):
 *   Constant: one value for the whole stream.
 *   Uniform: one value per curve. Meaningless for PrimitiveTopology::POINT_LIST
 *            (a point cloud has no sub-grouping smaller than the stream
 *            itself, so it is treated as Constant there).
 *   Varying: one value per point, or per curve vertex for curve topologies.
 */
enum class SpatialScope : uint8_t {
    Constant,
    Uniform,
    Varying,
};

/**
 * @struct SpatialAttribute
 * @brief One named, scoped channel of already-typed values.
 *
 * values holds one of Kakshya::DataVariant's float/glm::vec2/glm::vec3/
 * uint32_t vectors, sized per SpatialScope against the sample it is
 * attached to (1 for Constant, curve count for Uniform, point/vertex count
 * for Varying). Any other DataVariant alternative is rejected: those are
 * the only four with a native Alembic GeomParam counterpart
 * (OFloatGeomParam/OV2fGeomParam/OV3fGeomParam/OUInt32GeomParam).
 *
 * A vec3 named exactly "color" or "normal" is declared through Alembic's
 * dedicated color/normal GeomParam types instead of a plain vector one, so
 * an importer recognizes it as such rather than an arbitrary direction;
 * every other name, vec3 or otherwise, stays generic. MayaFlux's own
 * vertex fields already use these two names for these two roles, so this
 * costs a caller nothing to opt into.
 */
struct SpatialAttribute {
    std::string name;
    SpatialScope scope;
    Kakshya::DataVariant values;
};

/**
 * @struct SpatialSample
 * @brief One sample's worth of data for one named stream.
 *
 * topology decides which of the remaining fields apply:
 *   POINT_LIST: ids/velocities meaningful, vertex_counts_per_curve must be
 *               empty (every position is its own point).
 *   LINE_LIST / LINE_STRIP: vertex_counts_per_curve required (its sum must
 *               equal positions.size()), ids/velocities must be empty
 *               (Alembic's Curves schema has neither).
 * Any other topology is rejected: triangle data belongs to ModelWriter, not
 * this class.
 */
struct SpatialSample {
    Portal::Graphics::PrimitiveTopology topology;
    std::span<const glm::vec3> positions;
    std::span<const int32_t> vertex_counts_per_curve;
    std::span<const uint64_t> ids;
    std::span<const glm::vec3> velocities;
    std::span<const SpatialAttribute> attributes;
};

/**
 * @class SpatialCache
 * @brief Alembic-backed writer for time-sampled spatial entity state:
 *        particle systems, point clouds, inferred topology, and any other
 *        positions-plus-named-attributes-over-time source.
 *
 * Named after what it does, not the backend library or one data shape:
 * this is a cache of spatial state over time, not "the Alembic class."
 * Alembic is an implementation detail hidden behind Impl, the same
 * convention ModelReader uses to keep Assimp headers out of its own
 * public interface.
 *
 * Deliberately not registry-dispatched like ModelWriter/VolumeWriter: those
 * registries exist because several backend libraries could plausibly
 * implement the same format-agnostic contract. No second library writes
 * .abc files, so a registry here would be indirection with nothing to
 * dispatch between.
 *
 * Each named stream keeps its own Alembic sample count: calling write()
 * for a given stream_name appends one sample to that stream's own
 * timeline. There is no separate "advance time" step; Alembic's own
 * OTypedSchema::set() is what appends, and calling it once per capture
 * tick per stream already produces the sequence. Streams written at
 * different cadences (e.g. particles every frame, bonds only when they
 * change) simply end up with different sample counts on their own
 * timelines, which Alembic permits.
 *
 * Object-level metadata (write_metadata) is Alembic MetaData, fixed at the
 * point a stream's underlying object is first created and not
 * retroactively settable by Alembic's own API. Call write_metadata for a
 * stream_name before its first write() call for the tags to take effect;
 * calling it after is logged and ignored.
 *
 * Usage:
 * @code
 * IO::SpatialCache cache;
 * cache.open("swarm.abc");
 *
 * cache.write_metadata("particles", { { "operator", "PhysicsOperator" } });
 * cache.write("particles", {
 *     .topology = Portal::Graphics::PrimitiveTopology::POINT_LIST,
 *     .positions = positions,
 *     .ids = ids,
 *     .attributes = attributes,
 * });
 * // ... one write() call per capture tick ...
 *
 * cache.close();
 * @endcode
 */
class MAYAFLUX_API SpatialCache {
public:
    SpatialCache();
    ~SpatialCache();

    SpatialCache(const SpatialCache&) = delete;
    SpatialCache& operator=(const SpatialCache&) = delete;
    SpatialCache(SpatialCache&&) noexcept;
    SpatialCache& operator=(SpatialCache&&) noexcept;

    /**
     * @brief Open an Alembic archive for writing, Ogawa backend.
     * @param filepath Destination path. Anchored via resolve_write_path.
     * @return True on success. On failure call get_last_error().
     */
    bool open(const std::string& filepath);

    /**
     * @brief Append one sample to a named stream.
     *
     * Creates the stream's underlying Alembic object on first call for
     * this stream_name (applying any tags queued by write_metadata) and
     * declares each attribute's OGeomParam at that point. Subsequent calls
     * for the same stream_name must keep passing the same topology and the
     * same attributes (same names, types, scopes, same order): declaration
     * is one-time, only values change per sample.
     *
     * @param stream_name Name this stream is written under.
     * @param sample      This sample's data. See SpatialSample's own doc
     *                    for which fields apply to which topology.
     * @return True on success. On failure call get_last_error().
     */
    bool write(const std::string& stream_name, const SpatialSample& sample);

    /**
     * @brief Queue object-level metadata tags for a named stream.
     *
     * Must be called before that stream_name's first write() call: Alembic
     * MetaData is fixed at object creation and cannot be changed
     * afterward. A call arriving after the stream's object already exists
     * is logged and has no effect.
     *
     * @param stream_name Stream these tags apply to.
     * @param tags        Key-value pairs, e.g. run parameters
     *                    (gravity, bounds, rule shader name).
     * @return True if queued. False only if the stream's object was
     *         already created before this call.
     */
    bool write_metadata(
        const std::string& stream_name,
        const std::unordered_map<std::string, std::string>& tags);

    /**
     * @brief Finalize and close the archive.
     *
     * Safe to call on an unopened or already-closed cache; both are no-ops.
     */
    void close();

    [[nodiscard]] std::string get_last_error() const { return m_last_error; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    mutable std::string m_last_error;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }

    bool write_vertex_sample(const std::string& stream_name, const SpatialSample& sample);
    bool write_curves_sample(const std::string& stream_name, const SpatialSample& sample);
};

} // namespace MayaFlux::IO
