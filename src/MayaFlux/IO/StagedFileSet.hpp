#pragma once

namespace MayaFlux::IO {

/**
 * @class StagedFileSet
 * @brief Writes a set of files into a staging directory, then promotes them together.
 *
 * Each file is produced by a caller-supplied writer, so the set does not depend on
 * any format. Files are promoted in the order they were written. A caller that
 * needs one file to appear last, such as a manifest, writes it last. A set in
 * which any file failed to write is never promoted.
 *
 * The directory is resolved once with resolve_write_path. Staged files live in a
 * ".staging" subdirectory of it, which is removed on every exit from commit() and
 * on destruction. A stale ".staging" directory left by an earlier run is removed
 * when the set is constructed, so two sets on one directory must not overlap.
 *
 * Promotion is per file, not one atomic swap. If a rename fails partway, the files
 * promoted before it stay in place and commit() reports the failing name. Files in
 * the directory that are not part of the set are not touched.
 *
 * Usage:
 * @code
 * StagedFileSet set("states/run1");
 * set.write("fabric.exr", [&](const std::string& path) { return save_exr(path); }, error);
 * set.write("manifest.json", [&](const std::string& path) { return save_json(path); }, error);
 * if (!set.commit(error)) { ... }
 * @endcode
 */
class MAYAFLUX_API StagedFileSet {
public:
    using WriteFn = std::function<bool(const std::string& path)>;

    explicit StagedFileSet(const std::string& directory);
    ~StagedFileSet();

    StagedFileSet(const StagedFileSet&) = delete;
    StagedFileSet& operator=(const StagedFileSet&) = delete;
    StagedFileSet(StagedFileSet&&) = delete;
    StagedFileSet& operator=(StagedFileSet&&) = delete;

    /**
     * @brief Produce one file of the set in the staging directory.
     * @param name   Plain file name: nonempty, no path separators, not "." or "..",
     *               and not already written to this set. A name refused here
     *               writes nothing and leaves the set usable.
     * @param writer Called with the staged path. Returns true when the file is complete.
     * @param error  Receives the reason when false is returned.
     * @return True when the writer succeeded and the file exists in staging. A
     *         writer that fails, throws or leaves no file marks the set failed,
     *         because partial output may be in staging.
     */
    bool write(const std::string& name, const WriteFn& writer, std::string& error);

    /**
     * @brief Promote every staged file into the directory, in write order.
     * @param error Receives the reason when false is returned.
     * @return True when all files were promoted. False if no file was written,
     *         if any write failed, if the set was already committed, or if a
     *         rename failed.
     */
    bool commit(std::string& error);

    /** @brief Resolved target directory. */
    [[nodiscard]] const std::string& directory() const noexcept { return m_directory; }

    /** @brief Staging directory that holds files until commit. */
    [[nodiscard]] const std::string& staging_directory() const noexcept { return m_staging; }

private:
    std::string m_directory;
    std::string m_staging;
    std::vector<std::string> m_names;
    bool m_failed {};
    bool m_committed {};

    void remove_staging() noexcept;
};

}
