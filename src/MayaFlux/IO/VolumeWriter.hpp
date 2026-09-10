#pragma once

#include "MayaFlux/Kakshya/NDData/VolumeData.hpp"

namespace MayaFlux::IO {

/**
 * @struct VolumeWriteOptions
 * @brief Configuration for volume writing.
 *
 * Format-specific knobs are interpreted by the concrete writer; unsupported
 * options are silently ignored.
 */
struct VolumeWriteOptions {
    /**
     * @brief Magnitude below which a cell is written as inactive.
     *
     * Compared against the absolute value of a scalar and against the length
     * of a vector. Cells that fall below take the background value and cost
     * nothing in a sparse format, so this is the setting that decides file
     * size for a simulation field that is mostly empty.
     *
     * Applies to every field unless overridden in field_thresholds.
     */
    float activity_threshold { 1e-6F };

    /**
     * @brief Per-field overrides of activity_threshold, keyed by field name.
     *
     * A volume carrying density in [0, 1] alongside a pressure field spanning
     * several orders of magnitude has no single correct threshold. Names not
     * present here fall back to activity_threshold; names present but not
     * declared in the data are ignored without complaint.
     */
    std::unordered_map<std::string, float> field_thresholds;

    /**
     * @brief Value assumed by every inactive cell.
     *
     * Zero suits a density, a temperature, or any quantity whose absence is
     * an absence. A level set wants the signed narrow-band width instead, so
     * that space outside the stored band reads as far from the surface rather
     * than on it.
     */
    float background { 0.0F };

    /**
     * @brief Store values at half precision where the format supports it.
     *
     * Halves the payload. Adequate for anything destined for rendering,
     * inadequate for a field that will be simulated further downstream.
     */
    bool half_float { false };

    /**
     * @brief Format-specific compression code, or -1 for the writer's default.
     *
     * Matches the convention in ImageWriteOptions rather than enumerating
     * codes that differ per format.
     */
    int compression { -1 };
};

/**
 * @class VolumeWriter
 * @brief Abstract base for volumetric format writers.
 *
 * Parallels ImageWriter. Each concrete writer handles one or more file
 * extensions and is responsible for validating that the supplied VolumeData
 * is something its format can express: a format with no vector grid type
 * rejects a vector field rather than silently dropping a component.
 *
 * Writers are single-shot: one call to write() produces one file. A frame
 * sequence is a sequence of calls with different paths, since no volumetric
 * format in common use carries time within a single file.
 *
 * A writer receives host memory and nothing else. No Vulkan, no buffer, no
 * knowledge of what produced the values.
 */
class MAYAFLUX_API VolumeWriter {
public:
    virtual ~VolumeWriter() = default;

    /**
     * @brief Check whether this writer handles the given filepath.
     */
    [[nodiscard]] virtual bool can_write(const std::string& filepath) const = 0;

    /**
     * @brief Write volume data to disk.
     * @param filepath Destination path.
     * @param data     Volume data. Must satisfy VolumeData::is_consistent().
     * @param options  Format-specific options.
     * @return true on success. On failure call get_last_error().
     */
    virtual bool write(
        const std::string& filepath,
        const Kakshya::VolumeData& data,
        const VolumeWriteOptions& options = {}) = 0;

    /**
     * @brief File extensions handled by this writer (without dot).
     */
    [[nodiscard]] virtual std::vector<std::string> get_supported_extensions() const = 0;

    /**
     * @brief Last error message or empty string.
     */
    [[nodiscard]] virtual std::string get_last_error() const = 0;
};

using VolumeWriterFactory = std::function<std::unique_ptr<VolumeWriter>()>;

/**
 * @class VolumeWriterRegistry
 * @brief Singleton registry dispatching volume writes by file extension.
 *
 * Mirrors ImageWriterRegistry. Concrete writers register themselves during
 * subsystem init. create_writer(path) looks up the extension and returns a
 * fresh instance, or nullptr if none is registered.
 *
 * The nullptr is the whole point of the indirection: a format whose backing
 * library is not present on a given build simply has no entry, and the
 * caller gets a logged miss at the call site rather than a link error at
 * startup.
 */
class MAYAFLUX_API VolumeWriterRegistry {
public:
    static VolumeWriterRegistry& instance()
    {
        static VolumeWriterRegistry registry;
        return registry;
    }

    void register_writer(
        const std::vector<std::string>& extensions,
        const VolumeWriterFactory& factory)
    {
        for (const auto& ext : extensions) {
            m_factories[ext] = factory;
        }
    }

    [[nodiscard]] std::unique_ptr<VolumeWriter> create_writer(const std::string& filepath) const
    {
        auto ext = std::filesystem::path(filepath).extension().string();
        if (!ext.empty() && ext[0] == '.') {
            ext = ext.substr(1);
        }

        auto it = m_factories.find(ext);
        if (it != m_factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<std::string> get_registered_extensions() const
    {
        std::vector<std::string> exts;
        exts.reserve(m_factories.size());
        for (const auto& [ext, _] : m_factories) {
            exts.push_back(ext);
        }
        return exts;
    }

private:
    std::unordered_map<std::string, VolumeWriterFactory> m_factories;
};

} // namespace MayaFlux::IO
