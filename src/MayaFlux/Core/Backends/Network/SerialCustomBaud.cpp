#include "SerialCustomBaud.hpp"

#if defined(MAYAFLUX_PLATFORM_LINUX)

#include <asm/termbits.h>
#include <sys/ioctl.h>

namespace MayaFlux::Core::detail {

int set_custom_baud(int fd, uint32_t baud)
{
    termios2 settings {};
    if (::ioctl(fd, TCGETS2, &settings) != 0) {
        return errno;
    }

    settings.c_cflag &= ~static_cast<tcflag_t>(CBAUD);
    settings.c_cflag |= BOTHER;
    settings.c_ispeed = baud;
    settings.c_ospeed = baud;

    return ::ioctl(fd, TCSETS2, &settings) == 0 ? 0 : errno;
}

}

#elif defined(MAYAFLUX_PLATFORM_MACOS)

#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>

namespace MayaFlux::Core::detail {

int set_custom_baud(int fd, uint32_t baud)
{
    speed_t speed = baud;
    return ::ioctl(fd, IOSSIOSPEED, &speed) == 0 ? 0 : errno;
}

}

#else

namespace MayaFlux::Core::detail {

int set_custom_baud(int, uint32_t)
{
    return ENOTSUP;
}

}

#endif
