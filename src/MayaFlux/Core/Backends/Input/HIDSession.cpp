#include "HIDSession.hpp"

#include <hidapi.h>

namespace MayaFlux::Core {

HIDSession::HIDSession()
    : m_valid(hid_init() == 0)
{
}

HIDSession::~HIDSession()
{
    if (m_valid) {
        hid_exit();
    }
}

HIDBus HIDSession::bus_of(int bus_type)
{
    switch (bus_type) {
    case HID_API_BUS_USB:
        return HIDBus::USB;
    case HID_API_BUS_BLUETOOTH:
        return HIDBus::BLUETOOTH;
    case HID_API_BUS_I2C:
        return HIDBus::I2C;
    case HID_API_BUS_SPI:
        return HIDBus::SPI;
    default:
        return HIDBus::UNKNOWN;
    }
}

} // namespace MayaFlux::Core
