#include "StagedFileSet.hpp"

#include "MayaFlux/IO/FileWriter.hpp"

namespace MayaFlux::IO {

namespace {

    namespace fs = std::filesystem;

    bool valid_name(const std::string& name)
    {
        return !name.empty() && name != "." && name != ".."
            && name.find_first_of("/\\") == std::string::npos;
    }

}

StagedFileSet::StagedFileSet(const std::string& directory)
    : m_directory(resolve_write_path(directory))
    , m_staging((fs::path(m_directory) / ".staging").string())
{
    remove_staging();
}

StagedFileSet::~StagedFileSet()
{
    remove_staging();
}

bool StagedFileSet::write(const std::string& name, const WriteFn& writer, std::string& error)
{
    if (m_committed) {
        error = "staged set is already committed";
        return false;
    }
    if (m_failed) {
        error = "staged set has a failed write and cannot be promoted";
        return false;
    }
    if (!valid_name(name)) {
        error = "invalid staged file name: " + name;
        return false;
    }
    if (std::ranges::find(m_names, name) != m_names.end()) {
        error = "file already staged: " + name;
        return false;
    }

    std::error_code ec;
    fs::create_directories(m_staging, ec);
    if (ec) {
        error = "cannot create staging directory: " + ec.message();
        m_failed = true;
        return false;
    }

    const std::string path = (fs::path(m_staging) / name).string();
    bool written = false;
    try {
        written = writer && writer(path);
    } catch (const std::exception& e) {
        error = "writer threw for " + name + ": " + e.what();
        m_failed = true;
        return false;
    }

    if (!written || !fs::exists(path, ec)) {
        error = "writer failed for " + name;
        m_failed = true;
        return false;
    }

    m_names.push_back(name);
    return true;
}

bool StagedFileSet::commit(std::string& error)
{
    if (m_committed) {
        error = "staged set is already committed";
        return false;
    }
    if (m_failed || m_names.empty()) {
        error = m_failed ? "a staged write failed, nothing was promoted" : "no files were staged";
        m_failed = true;
        remove_staging();
        return false;
    }

    std::error_code ec;
    fs::create_directories(m_directory, ec);
    if (ec) {
        error = "cannot create target directory: " + ec.message();
        m_failed = true;
        remove_staging();
        return false;
    }

    for (const auto& name : m_names) {
        fs::rename(fs::path(m_staging) / name, fs::path(m_directory) / name, ec);
        if (ec) {
            error = "cannot promote " + name + ": " + ec.message();
            m_failed = true;
            remove_staging();
            return false;
        }
    }

    m_committed = true;
    remove_staging();
    return true;
}

void StagedFileSet::remove_staging() noexcept
{
    std::error_code ec;
    fs::remove_all(m_staging, ec);
}

}
