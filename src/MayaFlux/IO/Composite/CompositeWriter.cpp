#include "CompositeWriter.hpp"

namespace MayaFlux::IO {

bool CompositeWriter::write(
    const std::string& filepath,
    const Kakshya::CompositeArray& array)
{
    if (!open(filepath, array.layout()))
        return false;

    const auto rows = array.slice(0, array.size());
    const bool accepted = rows && write_rows(*rows);
    const bool finalized = close();

    return accepted && finalized;
}

}
