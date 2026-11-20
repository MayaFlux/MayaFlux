#pragma once

namespace MayaFlux::Core::detail {

/**
 * @brief Set a baud rate the portable serial option table does not accept
 * @param fd An open serial device.
 * @param baud The rate in bits per second.
 * @return 0 on success, otherwise an errno value. ENOTSUP where the platform has no such call.
 *
 * Linux sets it through the termios2 ioctls with BOTHER, and macOS through IOSSIOSPEED.
 * It lives in its own translation unit because the kernel's termios2 header cannot be
 * included beside the C library's termios.
 */
int set_custom_baud(int fd, uint32_t baud);

}
