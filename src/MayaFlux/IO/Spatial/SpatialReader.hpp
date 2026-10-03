#pragma once

#include "SpatialCache.hpp"

namespace MayaFlux::IO {

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
 * Only top level points and curves streams are read, which is everything
 * SpatialCache writes. Any other object in the file is skipped with a
 * warning. Attributes of types SpatialCache does not write are skipped with a
 * warning. A curves stream is reported as LINE_STRIP: a file does not tell a
 * list of pairs from curves of two vertices. Not safe to call from several
 * threads at once.
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

    [[nodiscard]] std::string get_last_error() const { return m_last_error; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    mutable std::string m_last_error;

    void set_error(std::string msg) const { m_last_error = std::move(msg); }
};

} // namespace MayaFlux::IO
