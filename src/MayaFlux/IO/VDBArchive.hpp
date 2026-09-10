#pragma once

#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"

namespace MayaFlux::IO::Detail {

/**
 * @struct VDBGridSpec
 * @brief One grid's worth of input to VDBArchive::add_grid.
 *
 * Values are the active cells only, parallel to coords by position: element
 * i of values belongs to coords[i]. Element width is four bytes for a scalar
 * grid and twelve for a vector grid, so values.size() must equal coords.size()
 * times that width.
 *
 * background is one element's worth of bytes, of the same width, used for
 * every inactive cell and every tile.
 *
 * metadata entries are written verbatim as string-typed grid metadata. The
 * caller owns those strings and they need only outlive the add_grid call.
 * The grid name is passed separately and does not need repeating here.
 */
struct VDBGridSpec {
    std::string_view name;
    std::span<const glm::ivec3> coords;
    std::span<const std::byte> values;
    std::span<const std::byte> background;
    bool is_vector { false };
    std::span<const std::pair<std::string, std::string>> metadata;
};

/**
 * @class VDBArchive
 * @brief RAII boundary around tinyvdb's C write path.
 *
 * Every tvdb_ symbol, every manual allocation and every C-interop lint
 * suppression in MayaFlux lives behind this class. Nothing above it includes
 * a tinyvdb header, so a future change of backing library touches one
 * translation unit.
 *
 * Grids accumulate through add_grid and are written together by a single
 * save, producing one .vdb carrying every field as a separately named grid.
 * The destructor releases whatever was built regardless of whether save ran,
 * so an abandoned archive and a failed one clean up identically.
 *
 * Not copyable and not movable: the underlying file struct holds a pointer
 * into this object's own grid storage.
 */
class VDBArchive {
public:
    VDBArchive();
    ~VDBArchive();

    VDBArchive(const VDBArchive&) = delete;
    VDBArchive& operator=(const VDBArchive&) = delete;
    VDBArchive(VDBArchive&&) = delete;
    VDBArchive& operator=(VDBArchive&&) = delete;

    /**
     * @brief Build one grid from active cells and retain it for saving.
     *
     * The lattice becomes a per-axis scale-translate transform mapping index
     * to world as cell_size() times index plus bounds.min plus half a cell,
     * which reproduces Lattice3D::cell_center exactly.
     *
     * @return False if the spec is inconsistent or the tree build failed.
     *         Call last_error() for detail. A failure leaves previously added
     *         grids intact.
     */
    bool add_grid(const Kinesis::Lattice3D& lattice, const VDBGridSpec& spec);

    /**
     * @brief Write every added grid to one file.
     *
     * @param path        Destination, already resolved by the caller.
     * @param compression Combination of VDBCompression flags.
     * @param level       Deflate level for the ZIP path, ignored otherwise.
     * @return False on failure; call last_error() for detail.
     */
    bool save(const std::string& path, uint32_t compression, int level);

    [[nodiscard]] size_t grid_count() const;
    [[nodiscard]] std::string_view last_error() const { return m_last_error; }

private:
    struct State;
    std::unique_ptr<State> m_state;
    std::string m_last_error;
};

/**
 * @brief Compression flags, mirroring tinyvdb's without exposing its header.
 *
 * Zip plus ActiveMask is the combination every DCC reads and the one that
 * needs no LZ4 path. Blosc produces smaller files and is what recent OpenVDB
 * writes by default.
 */
namespace VDBCompression {
    inline constexpr uint32_t None = 0x0;
    inline constexpr uint32_t Zip = 0x1;
    inline constexpr uint32_t ActiveMask = 0x2;
    inline constexpr uint32_t Blosc = 0x4;
    inline constexpr uint32_t Default = Zip | ActiveMask;
}

} // namespace MayaFlux::IO::Detail
