#pragma once

#include "MayaFlux/Core/GlobalInputConfig.hpp"

namespace MayaFlux::Core {

/**
 * @class HIDSession
 * @brief HIDAPI initialisation shared by every backend that uses it.
 *
 * The library is initialised on construction and released on destruction, so
 * it stays up while any holder remains. Hold mutex() around hid_enumerate,
 * hid_open_path, hid_close and hid_error(nullptr), which HIDAPI does not allow
 * to run concurrently, and never take it around reading.
 */
class MAYAFLUX_API HIDSession {
public:
    HIDSession();
    ~HIDSession();

    HIDSession(const HIDSession&) = delete;
    HIDSession& operator=(const HIDSession&) = delete;
    HIDSession(HIDSession&&) = delete;
    HIDSession& operator=(HIDSession&&) = delete;

    /** @brief True when HIDAPI initialised. */
    [[nodiscard]] bool valid() const { return m_valid; }

    /** @brief Lock for the calls named above, taken innermost. */
    [[nodiscard]] std::mutex& mutex() { return m_mutex; }

    /**
     * @brief Transport for a HIDAPI bus type value.
     * @param bus_type The bus_type of a hid_device_info.
     */
    [[nodiscard]] static HIDBus bus_of(int bus_type);

private:
    std::mutex m_mutex;
    bool m_valid { false };
};

} // namespace MayaFlux::Core
