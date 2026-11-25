#pragma once

#include "SpatialCache.hpp"

#include "MayaFlux/Kakshya/NDData/MeshAssemblyData.hpp"
#include "MayaFlux/Kinesis/Viewport/Lens.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
}

namespace MayaFlux::IO {

/**
 * @enum SpatialObjectKind
 * @brief What an object in an Alembic archive is. Group is an object with no schema,
 *        which only holds children; Other is a schema this reader does not know.
 */
enum class SpatialObjectKind : uint8_t {
    Group,
    Xform,
    Points,
    Curves,
    PolyMesh,
    SubD,
    Camera,
    Light,
    Material,
    FaceSet, ///< Part of the mesh above it.
    Other
};

/**
 * @struct SpatialObject
 * @brief One object of an archive's hierarchy.
 *
 * path joins the names from the root with '/', without a leading slash, so a top level
 * object's path is its name. parent indexes get_objects(); a top level object has none.
 * instance_source is the path of the object this one instances, empty when it is not an
 * instance root; instance_descendant is true under an instance root, where the file holds
 * no objects of its own.
 */
struct SpatialObject {
    std::string path;
    std::optional<size_t> parent;
    SpatialObjectKind kind {};
    size_t sample_count {};
    std::string instance_source;
    bool instance_descendant {};
};

/**
 * @class SpatialReader
 * @brief Reads back the time-sampled streams a SpatialCache wrote, in the
 *        same SpatialSample format it writes.
 *
 * Samples are read one at a time, on demand: nothing is loaded at open()
 * beyond stream headers, so an archive of any length can be walked frame by
 * frame.
 *
 * A sample returned by read_sample() borrows the reader's own storage for
 * that stream. It stays valid until the next read_sample() call for the same
 * stream or close(); copy what must outlive that. Samples of different
 * streams do not invalidate each other.
 *
 * Points and curves objects at any depth are streams, named by their path, so a
 * top level stream keeps its plain name, which is everything SpatialCache
 * writes. The rest of the hierarchy is listed by get_objects(), and transform
 * objects are read by read_transform(). Polygon and subdivision meshes are
 * meshes, not streams of points: get_mesh_names() lists them and read_mesh()
 * reads one sample as a MeshData.
 * Attributes of types SpatialCache does not write are skipped with a
 * warning. Points and curves widths, and curves uvs and normals, arrive as the
 * attributes "width", "uv" and "normal". Texture coordinates of meshes and curves
 * are turned from Alembic's upward v to the downward v the engine uses. A curves
 * stream is reported as LINE_STRIP: a file does not tell a list of pairs from
 * curves of two vertices. Not safe to call from several threads at once.
 *
 * Usage:
 * @code
 * IO::SpatialReader reader;
 * reader.open("swarm.abc");
 * for (const auto& name : reader.get_stream_names()) {
 *     for (size_t i = 0; i < reader.get_sample_count(name); ++i) {
 *         auto sample = reader.read_sample(name, i);
 *     }
 * }
 * @endcode
 */
class MAYAFLUX_API SpatialReader {
public:
    SpatialReader();
    ~SpatialReader();

    SpatialReader(const SpatialReader&) = delete;
    SpatialReader& operator=(const SpatialReader&) = delete;
    SpatialReader(SpatialReader&&) noexcept;
    SpatialReader& operator=(SpatialReader&&) noexcept;

    /**
     * @brief Open an Ogawa or HDF5 archive and index its streams.
     *
     * The file is checked first: a missing path or a file that is not an
     * archive fails with one logged line, without Alembic being involved.
     *
     * @param filepath Path to the file. Resolved the same way other readers do.
     * @return True on success. On failure the error is logged and available
     *         from get_last_error().
     */
    bool open(const std::string& filepath);

    /**
     * @brief Release the archive. Safe on an unopened reader.
     */
    void close();

    [[nodiscard]] bool is_open() const;

    /**
     * @brief Names of the streams found at open(), in file order.
     */
    [[nodiscard]] const std::vector<std::string>& get_stream_names() const;

    /**
     * @brief Topology a stream is read as: POINT_LIST or LINE_STRIP.
     * @return nullopt for an unknown stream.
     */
    [[nodiscard]] std::optional<Portal::Graphics::PrimitiveTopology> get_topology(
        const std::string& stream_name) const;

    /**
     * @brief Number of samples in a stream, 0 for an unknown stream.
     */
    [[nodiscard]] size_t get_sample_count(const std::string& stream_name) const;

    /**
     * @brief Object level tags a stream was written with, the same form
     *        SpatialCache::write_metadata takes. Empty for an unknown stream.
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata(
        const std::string& stream_name) const;

    /**
     * @brief Read one sample of a stream.
     *
     * Fields the format does not carry for the stream's topology are empty:
     * ids and velocities for a curves stream, vertex_counts_per_curve for
     * points, and ids or velocities a points stream was written without.
     *
     * @param stream_name Name from get_stream_names().
     * @param index       Sample index, below get_sample_count().
     * @return The sample, valid as described in the class doc, or nullopt for
     *         an unknown stream, an index out of range or a read failure; the
     *         reason is in get_last_error(). An unknown stream or index is
     *         not logged.
     */
    [[nodiscard]] std::optional<SpatialSample> read_sample(
        const std::string& stream_name, size_t index) const;

    /**
     * @brief Time in seconds the archive assigns to a sample.
     * @return The time, or nullopt if the stream or index is invalid.
     */
    [[nodiscard]] std::optional<double> sample_time(
        const std::string& stream_name, size_t index) const;

    /**
     * @brief Number of vertices a sample holds, read from the size of its positions.
     *
     * Nothing of the sample is decoded, so it is cheap to ask of every sample of a stream.
     *
     * @return The count, or nullopt for an unknown stream, an index out of range or a
     *         read failure; a failure is in get_last_error().
     */
    [[nodiscard]] std::optional<size_t> get_vertex_count(
        const std::string& stream_name, size_t index) const;

    /**
     * @brief Paths of the polygon and subdivision mesh objects, in file order.
     */
    [[nodiscard]] const std::vector<std::string>& get_mesh_names() const;

    /**
     * @brief Number of samples of a mesh object, 0 for an unknown one.
     */
    [[nodiscard]] size_t get_mesh_sample_count(const std::string& mesh_name) const;

    /**
     * @brief How a mesh object's topology changes over its samples, as the file reports it.
     * @return nullopt for an unknown mesh.
     */
    [[nodiscard]] std::optional<Kakshya::TopologyVariance> get_mesh_variance(
        const std::string& mesh_name) const;

    /**
     * @brief Time in seconds the archive assigns to a sample of a mesh object.
     * @return The time, or nullopt if the mesh or index is invalid.
     */
    [[nodiscard]] std::optional<double> mesh_time(const std::string& mesh_name, size_t index) const;

    /**
     * @brief Paths of the camera objects, in file order.
     */
    [[nodiscard]] const std::vector<std::string>& get_camera_names() const;

    /**
     * @brief Number of samples of a camera object, 0 for an unknown one.
     */
    [[nodiscard]] size_t get_camera_sample_count(const std::string& camera_name) const;

    /**
     * @brief Time in seconds the archive assigns to a sample of a camera object.
     * @return The time, or nullopt if the camera or index is invalid.
     */
    [[nodiscard]] std::optional<double> camera_time(const std::string& camera_name, size_t index) const;

    /**
     * @brief Read one sample of a camera object as a lens.
     *
     * Focal length, film back, clip planes, f-stop and focus distance as the file wrote them, and the
     * screen window Alembic works out from the film offset, overscan, lens squeeze and film back
     * transform (CameraSample::getScreenWindow). The camera's own placement is not in the lens: it is
     * the world placement of the camera's row, read through the transform objects above it.
     *
     * @param camera_name Path from get_camera_names().
     * @param index Sample index, below get_camera_sample_count().
     * @return The lens, or nullopt for an unknown camera, an index out of range or a read failure;
     *         the reason is in get_last_error().
     */
    [[nodiscard]] std::optional<Kinesis::Lens> read_lens(const std::string& camera_name, size_t index) const;

    /**
     * @brief Read one sample of a mesh object as triangles.
     *
     * Alembic winds polygons clockwise in a right handed system, so the corners of every face
     * are reversed to give the counter clockwise winding the engine's meshes use. Faces of any
     * size are triangulated. A subdivision surface is read as its control cage and is not
     * subdivided.
     *
     * The first colour parameter becomes the vertex colour. Further colour and uv sets and
     * other arbitrary parameters are kept by create_mesh_assembly(), not by this call.
     *
     * Normals and texture coordinates are taken when the sample has them per vertex or per
     * face corner. A mesh that has any per corner values becomes one vertex per corner; other
     * scopes are skipped with a warning. Normals the file lacks are derived from the faces, and
     * tangents from the texture coordinates when there are some. The result owns its data.
     *
     * @param mesh_name Path from get_mesh_names().
     * @param index Sample index, below get_mesh_sample_count().
     * @return The mesh, or nullopt for an unknown mesh, an index out of range, a sample whose
     *         faces do not fit its vertices or a read failure; the reason is in get_last_error().
     */
    [[nodiscard]] std::optional<Kakshya::MeshData> read_mesh(
        const std::string& mesh_name, size_t index) const;

    /**
     * @brief Every object of the archive in file order, parents before children.
     */
    [[nodiscard]] const std::vector<SpatialObject>& get_objects() const;

    /**
     * @brief The matrix of a transform object at a sample, in the convention of the
     *        MeshAssembly: it places the object's children relative to its parent.
     * @param path Path of an object of kind Xform.
     * @param index Sample index, below the object's sample_count.
     * @return The matrix, or nullopt for an unknown path, a path that is not a transform,
     *         an index out of range or a read failure.
     */
    [[nodiscard]] std::optional<glm::mat4> read_transform(
        const std::string& path, size_t index) const;

    /**
     * @brief Time in seconds the archive assigns to a sample of a transform object.
     * @return The time, or nullopt for a path that is not a transform or an index out of range.
     */
    [[nodiscard]] std::optional<double> transform_time(
        const std::string& path, size_t index) const;

    /**
     * @brief Build a MeshAssemblyContainer from the archive's hierarchy.
     *
     * Transform objects and groups become placement rows, parented as the archive is; a
     * transform that does not inherit its parent's transform becomes a root. Each points,
     * curves, polygon or subdivision mesh object becomes a source holding its first sample,
     * with its sample times and its topology variance, and a row that shows it. An instance
     * shares the sources of the object it instances instead of reading them again. A
     * transform that changes over time becomes a channel of one motion named for the file.
     *
     * Lights become sources too. Materials become the assembly's materials, assigned to the
     * rows that show geometry, inherited down the hierarchy. A group holding only materials
     * gets no row. A mesh source keeps in its facets whatever its vertices cannot hold: the
     * further vertex sets, MeshAssemblyAttributes, MeshAssemblyPointMap, MeshAssemblyFaceSets
     * and, for a subdivision surface, MeshAssemblySubdivision. A curves source keeps its
     * MeshAssemblyCurveForm. The assembly's facets hold user properties as
     * MeshAssemblyMetadata and explicit visibility as MeshAssemblyVisibility.
     *
     * NURBS surfaces and shader networks are not read.
     *
     * The variance of a mesh or curves object is the file's; that of a points object is
     * Constant for one sample and Heterogeneous for more, since the point count may change.
     *
     * @return The assembly, or nullptr when no archive is open.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::MeshAssemblyContainer> create_mesh_assembly() const;

    [[nodiscard]] std::string get_last_error() const { return m_last_error; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    mutable std::string m_last_error;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
