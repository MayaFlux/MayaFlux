#pragma once

#include "StreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Marker interface for containers backed by file storage (in-memory only).
 *
 * Carries the origin path and format string populated by IO classes at load
 * time. All disk I/O, decoding, and metadata operations remain in IO classes.
 * Uses virtual inheritance to support the diamond inheritance pattern with
 * SoundStreamContainer and VideoStreamContainer.
 */
class MAYAFLUX_API FileContainer : public virtual StreamContainer {
public:
    virtual ~FileContainer() = default;

    /**
     * @brief Absolute path of the file this container was loaded from.
     * @return Empty string if the container was not loaded from a file.
     */
    [[nodiscard]] const std::string& get_source_path() const { return m_source_path; }

    /**
     * @brief Short format identifier as reported by the demuxer (e.g. "wav", "mp4", "flac").
     * @return Empty string if not set.
     */
    [[nodiscard]] const std::string& get_source_format() const { return m_source_format; }

    /**
     * @brief Set the origin file path. Called by IO classes after a successful load.
     * @param path Absolute or relative path used to open the file.
     */
    void set_source_path(std::string path) { m_source_path = std::move(path); }

    /**
     * @brief Set the format identifier. Called by IO classes after a successful load.
     * @param fmt Short format string from the demuxer context.
     */
    void set_source_format(std::string fmt) { m_source_format = std::move(fmt); }

protected:
    std::string m_source_path;
    std::string m_source_format;
};

} // namespace MayaFlux::Kakshya
