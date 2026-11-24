#pragma once

#include "SpatialReader.hpp"

namespace MayaFlux::IO {

/**
 * @class SpatialArchive
 * @brief One open Alembic archive that any number of frame sources read through.
 *
 * A SpatialReader is not thread safe, and opening the archive again for every object is what a
 * scene of many animated objects cannot afford. The archive holds one reader behind one lock, and
 * the frame sources made over it (MeshFileFrames, SpatialFileFrames) take turns on it, so a file
 * is opened and its hierarchy indexed once however many of its objects play.
 */
class MAYAFLUX_API SpatialArchive {
public:
    /** @return nullptr, with the reason logged, for a file that cannot be read. */
    [[nodiscard]] static std::shared_ptr<SpatialArchive> open(const std::string& path);

    [[nodiscard]] const std::string& path() const noexcept { return m_path; }

    /**
     * @brief Run a function on the reader while nothing else reads the archive.
     * @param fn Called with the SpatialReader. It must not read the same archive again.
     * @return What fn returns.
     */
    template <typename Fn>
    decltype(auto) read(Fn&& fn) const
    {
        const std::scoped_lock lock(m_mutex);
        return std::forward<Fn>(fn)(m_reader);
    }

private:
    SpatialArchive(SpatialReader reader, std::string path)
        : m_reader(std::move(reader))
        , m_path(std::move(path))
    {
    }

    mutable SpatialReader m_reader;
    mutable std::mutex m_mutex;
    std::string m_path;
};

} // namespace MayaFlux::IO
