#include "SpatialArchive.hpp"

namespace MayaFlux::IO {

std::shared_ptr<SpatialArchive> SpatialArchive::open(const std::string& path)
{
    SpatialReader reader;
    if (!reader.open(path)) {
        return nullptr;
    }
    return std::shared_ptr<SpatialArchive>(new SpatialArchive(std::move(reader), path));
}

} // namespace MayaFlux::IO
