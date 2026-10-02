#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/ImageData.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Transitive/Memory/SweepList.hpp"

namespace MayaFlux::Buffers {

/**
 * @brief Reads selected buffer storage into owned host data, one snapshot per request.
 *
 * Each read_* call arms one request that runs on the next graphics cycle; the
 * matching resolve_* call finishes it on the consuming thread, waiting for a
 * device-local copy when one is in flight. Arm again after resolving to read
 * every frame.
 *
 * Geometry: read_vertices, read_mesh. Pixels: read_host_pixels,
 * read_texture_pixels. Data: read_bytes, read_buffer, read_field,
 * read_back_buffer.
 */
class MAYAFLUX_API DataReadProcessor : public VKBufferProcessor {
public:
    DataReadProcessor();
    ~DataReadProcessor() override;

    /**
     * @brief Read the attached buffer's primary storage.
     * @param byte_count Bytes to read, or zero for the full allocation.
     */
    void read_bytes(size_t byte_count = 0);

    /**
     * @brief Read another VKBuffer, such as a descriptor-bound or instance buffer.
     * @param source Buffer to read; retained until the request runs.
     * @param byte_count Bytes to read, or zero for its full allocation, which
     *        may exceed the bytes its owner currently fills.
     */
    void read_buffer(std::shared_ptr<VKBuffer> source, size_t byte_count = 0);

    /**
     * @brief Read the current read slot of a named field on the attached buffer.
     *
     * Resolves a VolumeGridBuffer field or a NetworkGeometryBuffer state field,
     * at the time the request runs so a swapped generation is not read stale.
     */
    void read_field(std::string name);

    /**
     * @brief Read a raw back_buffers slot of the attached buffer.
     * @param index Slot index into its back_buffers.
     * @param byte_count Bytes to read. A slot does not carry its size, so this
     *        must not exceed what the buffer's owner allocated for it.
     */
    void read_back_buffer(size_t index, size_t byte_count);

    /** @brief Read the attached buffer's declared vertices; the layout count may represent capacity. */
    void read_vertices();

    /**
     * @brief Read the attached mesh.
     *
     * A MeshBuffer, a GeometryBuffer driven by a MeshWriterNode, and a
     * MeshNetworkBuffer yield a copy of their CPU-authoritative geometry with
     * no transfer; a network is merged into one mesh with a submesh per slot,
     * in local space with no transforms applied. A ComputeMeshBuffer or a
     * VolumeGridBuffer with a surface yields its live vertices as non-indexed
     * triangles, sized by its live counter rather than its capacity.
     */
    void read_mesh();

    /** @brief Read the host pixel data a TextureBuffer retains. */
    void read_host_pixels();

    /**
     * @brief Download the base level of a TextureBuffer or NodeTextureBuffer image.
     *
     * Blocks the graphics cycle for the transfer, reusing one staging buffer.
     */
    void read_texture_pixels();

    /** @brief Whether a requested read has been submitted. */
    [[nodiscard]] bool has_result() const noexcept;

    /** @brief Whether a read is armed, or its result has not been resolved yet. */
    [[nodiscard]] bool is_pending() const noexcept;

    /**
     * @brief Drop read_* calls while a read is armed or unresolved.
     *
     * By default a newer read replaces an armed one that has not run yet. With
     * this set the call is dropped instead, so a caller that re-arms every tick
     * never displaces a read still in flight. Best effort across the brief
     * moment a read is running. Set it and call read_* from the consuming thread.
     */
    void set_skip_while_pending(bool skip) noexcept { m_skip_while_pending = skip; }

    /**
     * @brief Finish the read as raw bytes.
     *
     * Waits for a device-local transfer when necessary. Call outside the
     * graphics processing thread. A pixel read yields its raw pixel bytes.
     * Returns nullopt if no result is pending or the pending result is a
     * MeshBuffer copy; use resolve_mesh() for that.
     */
    [[nodiscard]] std::optional<Kakshya::DataVariant> resolve_bytes();

    /**
     * @brief Finish a read_vertices() read as typed channels.
     *
     * One DataVariant per layout attribute, in layout order (see VertexInsertion).
     * Returns nullopt, leaving the result pending, when the pending read was
     * not a vertex read; returns nullopt and discards it when its layout has
     * an attribute that cannot be decoded. resolve_bytes() still yields the raw
     * interleaved bytes of any vertex read.
     */
    [[nodiscard]] std::optional<std::vector<Kakshya::DataVariant>> resolve_vertices();

    /**
     * @brief Finish a read_mesh() read as MeshData.
     *
     * Returns nullopt, leaving the result pending, when the pending read was
     * not a mesh read.
     */
    [[nodiscard]] std::optional<Kakshya::MeshData> resolve_mesh();

    /**
     * @brief Finish a pixel read as typed ImageData with its format and dimensions.
     *
     * Returns nullopt, leaving the result pending, when the pending read was
     * not a pixel read.
     */
    [[nodiscard]] std::optional<Kakshya::ImageData> resolve_pixels();

    [[nodiscard]] bool is_compatible_with(const std::shared_ptr<Buffer>& buffer) const override;

protected:
    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

private:
    struct ReadRequest;
    struct PendingRead;
    enum class ReadKind { Bytes, Vertices, Mesh, Pixels };

    using ReadFill = std::function<bool(DataReadProcessor&, const std::shared_ptr<VKBuffer>&, PendingRead&)>;

    void arm(ReadFill fill);

    static bool submit_read(
        PendingRead& pending,
        std::shared_ptr<VKBuffer>& staging,
        size_t available_bytes,
        size_t byte_count,
        bool mapped,
        bool transferable);

    std::unique_ptr<PendingRead> collect_result(ReadKind kind);

    Memory::SweepList<ReadRequest> m_request;
    Memory::SweepList<std::unique_ptr<PendingRead>> m_result;
    std::atomic<bool> m_result_occupied { false };
    std::atomic<bool> m_result_ready { false };
    std::shared_ptr<VKBuffer> m_staging;
    std::shared_ptr<VKBuffer> m_texture_staging;
    bool m_skip_while_pending {};
};

}
