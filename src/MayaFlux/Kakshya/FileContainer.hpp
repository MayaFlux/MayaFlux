#pragma once

#include "StreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Marker interface for containers backed by file storage (in-memory only).
 *
 * All disk I/O, file format, and metadata operations must be handled by IO classes.
 * This interface exists for semantic clarity and future extension.
 */
class FileContainer : public StreamContainer {
public:
    virtual ~FileContainer() = default;
    // No additional methods; all functionality is inherited or delegated.
};

} // namespace MayaFlux::Kakshya
