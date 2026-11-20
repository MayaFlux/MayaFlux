#include "SerialBackend.hpp"

#include "StreamBackend.inl"

#if !defined(MAYAFLUX_PLATFORM_WINDOWS)
#include "SerialCustomBaud.hpp"

#include <sys/ioctl.h>
#include <termios.h>
#endif

namespace MayaFlux::Core {

namespace {

    using Port = SerialTransport::Stream;

    asio::error_code apply_lines(Port& port, std::optional<bool> dtr, std::optional<bool> rts)
    {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        const HANDLE handle = port.native_handle();
        if (dtr && !EscapeCommFunction(handle, *dtr ? SETDTR : CLRDTR)) {
            return { static_cast<int>(GetLastError()), asio::system_category() };
        }
        if (rts && !EscapeCommFunction(handle, *rts ? SETRTS : CLRRTS)) {
            return { static_cast<int>(GetLastError()), asio::system_category() };
        }
#else
        const int fd = port.native_handle();
        const auto drive = [fd](std::optional<bool> level, int line) -> asio::error_code {
            if (level && ::ioctl(fd, *level ? TIOCMBIS : TIOCMBIC, &line) != 0) {
                return { errno, asio::system_category() };
            }
            return {};
        };
        if (const auto ec = drive(dtr, TIOCM_DTR)) {
            return ec;
        }
        if (const auto ec = drive(rts, TIOCM_RTS)) {
            return ec;
        }
#endif
        return {};
    }

    asio::serial_port_base::parity::type parity_of(SerialParity parity)
    {
        switch (parity) {
        case SerialParity::ODD:
            return asio::serial_port_base::parity::odd;
        case SerialParity::EVEN:
            return asio::serial_port_base::parity::even;
        case SerialParity::NONE:
            break;
        }
        return asio::serial_port_base::parity::none;
    }

    asio::serial_port_base::stop_bits::type stop_bits_of(SerialStopBits stop_bits)
    {
        switch (stop_bits) {
        case SerialStopBits::ONE_POINT_FIVE:
            return asio::serial_port_base::stop_bits::onepointfive;
        case SerialStopBits::TWO:
            return asio::serial_port_base::stop_bits::two;
        case SerialStopBits::ONE:
            break;
        }
        return asio::serial_port_base::stop_bits::one;
    }

    asio::serial_port_base::flow_control::type flow_control_of(SerialFlowControl flow)
    {
        switch (flow) {
        case SerialFlowControl::SOFTWARE:
            return asio::serial_port_base::flow_control::software;
        case SerialFlowControl::HARDWARE:
            return asio::serial_port_base::flow_control::hardware;
        case SerialFlowControl::NONE:
            break;
        }
        return asio::serial_port_base::flow_control::none;
    }

} // namespace

asio::error_code SerialTransport::open(Stream& port, const EndpointInfo& info)
{
    const SerialLineSettings& line = info.serial;
    if (info.remote_path.empty() || line.data_bits < 5 || line.data_bits > 8) {
        return asio::error::invalid_argument;
    }

    asio::error_code ec;
    port.open(info.remote_path, ec);
    if (ec) {
        return ec;
    }

    const auto fail = [&port](const asio::error_code& cause) {
        asio::error_code ignored;
        port.close(ignored);
        return cause;
    };

    port.set_option(asio::serial_port_base::character_size(line.data_bits), ec);
    if (ec) {
        return fail(ec);
    }
    port.set_option(asio::serial_port_base::parity(parity_of(line.parity)), ec);
    if (ec) {
        return fail(ec);
    }
    port.set_option(asio::serial_port_base::stop_bits(stop_bits_of(line.stop_bits)), ec);
    if (ec) {
        return fail(ec);
    }
    port.set_option(asio::serial_port_base::flow_control(flow_control_of(line.flow_control)), ec);
    if (ec) {
        return fail(ec);
    }

    port.set_option(asio::serial_port_base::baud_rate(line.baud_rate), ec);
#if !defined(MAYAFLUX_PLATFORM_WINDOWS)
    if (ec == asio::error::invalid_argument) {
        const int error = detail::set_custom_baud(port.native_handle(), line.baud_rate);
        ec = error == 0 ? asio::error_code {} : asio::error_code(error, asio::system_category());
    }
#endif
    if (ec) {
        return fail(ec);
    }

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    ec = apply_lines(port, line.dtr_on_open, line.rts_on_open);
#else
    ec = apply_lines(port,
        line.dtr_on_open ? std::nullopt : std::optional<bool> { false },
        line.rts_on_open ? std::nullopt : std::optional<bool> { false });
#endif
    if (ec) {
        return fail(ec);
    }

    return {};
}

std::chrono::milliseconds SerialTransport::settle_time(const EndpointInfo& info)
{
    return std::chrono::milliseconds(info.serial.settle_ms);
}

void SerialTransport::ready(Stream& port, const EndpointInfo&)
{
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    PurgeComm(port.native_handle(), PURGE_RXCLEAR);
#else
    ::tcflush(port.native_handle(), TCIFLUSH);
#endif
}

bool SerialTransport::set_control_lines(Stream& port, std::optional<bool> dtr, std::optional<bool> rts)
{
    return !apply_lines(port, dtr, rts);
}

bool SerialTransport::send_break(Stream& port)
{
    asio::error_code ec;
    static_cast<void>(port.send_break(ec));
    return !ec;
}

std::string SerialTransport::describe(const EndpointInfo& info)
{
    return info.remote_path;
}

std::string SerialTransport::peer_label(const EndpointInfo& info)
{
    return info.remote_path;
}

template class MAYAFLUX_API StreamBackend<SerialTransport>;

} // namespace MayaFlux::Core
