#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace MayaFlux::Test {

/**
 * @brief Switches into a fresh temporary directory for the scope's lifetime.
 *
 * The previous working directory is restored and the temporary one removed on
 * destruction.
 */
class ScopedWorkingDir {
public:
    ScopedWorkingDir()
        : m_previous(std::filesystem::current_path())
        , m_root(std::filesystem::temp_directory_path()
              / ("mf_cwd_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directories(m_root);
        std::filesystem::current_path(m_root);
    }

    ~ScopedWorkingDir()
    {
        std::error_code ec;
        std::filesystem::current_path(m_previous, ec);
        std::filesystem::remove_all(m_root, ec);
    }

    ScopedWorkingDir(const ScopedWorkingDir&) = delete;
    ScopedWorkingDir& operator=(const ScopedWorkingDir&) = delete;
    ScopedWorkingDir(ScopedWorkingDir&&) = delete;
    ScopedWorkingDir& operator=(ScopedWorkingDir&&) = delete;

    void write(const std::filesystem::path& relative, const std::string& content) const
    {
        const auto target = m_root / relative;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream(target) << content;
    }

private:
    std::filesystem::path m_previous;
    std::filesystem::path m_root;
};

}
