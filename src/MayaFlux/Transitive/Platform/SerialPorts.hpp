#pragma once

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif
#endif

namespace MayaFlux::Platform {

/**
 * @struct SerialPortEntry
 * @brief A serial port the system exposes.
 *
 * name is stable across a replug where the system offers a stable name, and path is what
 * to open. On Linux a port with a /dev/serial/by-id link is named and opened by that link,
 * and one without by its device node.
 */
struct SerialPortEntry {
    std::filesystem::path path;
    std::string name;
};

/**
 * @brief The serial ports the system exposes, sorted by name.
 *
 * Linux: /dev/ttyACM* and /dev/ttyUSB*. macOS: /dev/cu.*. Windows: the ports in the
 * SERIALCOMM registry key, such as COM3. A system that exposes none gives an empty list,
 * and a port can always be opened by typing its path.
 */
[[nodiscard]] inline std::vector<SerialPortEntry> list_serial_ports()
{
    std::vector<SerialPortEntry> ports;

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    HKEY key {};
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        for (DWORD index = 0;; ++index) {
            char value_name[256] {};
            DWORD value_name_size = sizeof(value_name);
            char data[256] {};
            DWORD data_size = sizeof(data);
            DWORD type = 0;
            const LONG status = RegEnumValueA(key, index, value_name, &value_name_size, nullptr, &type,
                reinterpret_cast<LPBYTE>(data), &data_size);
            if (status == ERROR_NO_MORE_ITEMS) {
                break;
            }
            if (status != ERROR_SUCCESS || type != REG_SZ) {
                continue;
            }
            const std::string port(data, std::find(data, data + data_size, '\0'));
            if (!port.empty()) {
                ports.push_back({ .path = port, .name = port });
            }
        }
        RegCloseKey(key);
    }
#elif defined(MAYAFLUX_PLATFORM_MACOS)
    std::error_code ec;
    for (std::filesystem::directory_iterator entry("/dev", ec), end; !ec && entry != end; entry.increment(ec)) {
        const std::string file = entry->path().filename().string();
        if (file.starts_with("cu.")) {
            ports.push_back({ .path = entry->path(), .name = file });
        }
    }
#else
    std::map<std::filesystem::path, std::filesystem::path> stable;
    std::error_code ec;
    for (std::filesystem::directory_iterator entry("/dev/serial/by-id", ec), end; !ec && entry != end; entry.increment(ec)) {
        std::error_code resolve_ec;
        const auto target = std::filesystem::canonical(entry->path(), resolve_ec);
        if (!resolve_ec) {
            stable[target] = entry->path();
        }
    }

    ec.clear();
    for (std::filesystem::directory_iterator entry("/dev", ec), end; !ec && entry != end; entry.increment(ec)) {
        const std::string file = entry->path().filename().string();
        if (!file.starts_with("ttyACM") && !file.starts_with("ttyUSB")) {
            continue;
        }
        std::error_code resolve_ec;
        const auto target = std::filesystem::canonical(entry->path(), resolve_ec);
        const auto link = resolve_ec ? stable.end() : stable.find(target);
        if (link != stable.end()) {
            ports.push_back({ .path = link->second, .name = link->second.filename().string() });
        } else {
            ports.push_back({ .path = entry->path(), .name = file });
        }
    }
#endif

    std::ranges::sort(ports, [](const SerialPortEntry& a, const SerialPortEntry& b) { return a.name < b.name; });
    return ports;
}

} // namespace MayaFlux::Platform
