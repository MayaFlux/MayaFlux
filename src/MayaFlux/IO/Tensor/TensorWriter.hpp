#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"

namespace MayaFlux::IO {

/**
 * @class TensorWriter
 * @brief Single-shot writer of a TensorSet to one file format.
 *
 * Tensors are written at their native element type in dense C order, so
 * strided tensors are gathered first. A format that cannot represent a tensor
 * (an element type it has no name for, a name it cannot store) refuses the
 * whole set and reports why through get_last_error().
 */
class MAYAFLUX_API TensorWriter {
public:
    virtual ~TensorWriter() = default;

    /**
     * @brief Write every tensor in the set to a file.
     * @param filepath Destination path.
     * @param tensors Tensors with unique, non-empty names.
     * @return True only if the complete file was written.
     */
    virtual bool write(const std::string& filepath, const Kakshya::TensorSet& tensors) = 0;

    /** @brief Reason the last write failed, or empty. */
    [[nodiscard]] virtual std::string get_last_error() const = 0;

    /** @brief Extensions this format writes, without dots. */
    [[nodiscard]] virtual std::vector<std::string> get_supported_extensions() const = 0;
};

}
