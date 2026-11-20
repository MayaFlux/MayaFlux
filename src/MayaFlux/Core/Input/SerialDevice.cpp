#include "SerialDevice.hpp"

namespace MayaFlux::Core {

namespace P = Transitive::Protocol;

SerialDevice::SerialDevice(uint32_t device_id, SerialSpec spec, std::unique_ptr<ISerialLink> link)
    : m_device_id(device_id)
    , m_spec(std::move(spec))
    , m_link(std::move(link))
    , m_reader(std::make_unique<SerialReader>(device_id, m_spec, *m_link))
{
    m_reader->set_frame_filter([this](P::Message frame) { return complete_request(frame); });
    m_link->set_chunk_callback([this](std::span<const uint8_t> chunk) { handle_chunk(chunk); });
    m_link->set_state_callback([this](EndpointState previous, EndpointState current) {
        handle_state(previous, current);
    });
}

SerialDevice::~SerialDevice()
{
    m_link->close();
    if (const auto dropped = take_pending()) {
        if (const auto token = dropped->timer.exchange(0); token != 0) {
            m_link->cancel_post(token);
        }
    }
}

void SerialDevice::set_value_callback(ValueCallback callback)
{
    m_reader->set_value_callback(std::move(callback));
}

SerialDevice::ReaderToken SerialDevice::add_reader(SerialSpec spec, uint32_t id, ValueCallback callback)
{
    auto reader = std::make_shared<SerialReader>(id, std::move(spec), *m_link);
    reader->set_value_callback(std::move(callback));
    const ReaderToken token = m_next_token.fetch_add(1);
    m_readers.update([&](ReaderList& list) {
        list.emplace_back(token, std::move(reader));
        return true;
    });
    return token;
}

void SerialDevice::remove_reader(ReaderToken token)
{
    std::shared_ptr<SerialReader> removed;
    m_readers.update([&](ReaderList& list) {
        removed.reset();
        const auto it = std::ranges::find(list, token, &ReaderList::value_type::first);
        if (it == list.end()) {
            return false;
        }
        removed = std::move(it->second);
        list.erase(it);
        return true;
    });
}

void SerialDevice::set_state_callback(StateCallback callback)
{
    m_on_state = std::move(callback);
}

bool SerialDevice::open(const std::string& path)
{
    m_reader->reset();
    return m_link->open(path, m_spec.line);
}

void SerialDevice::close()
{
    m_link->close();
    end_request(take_pending(), SerialReply::LINK_LOST, {});
}

EndpointState SerialDevice::state() const
{
    return m_link->state();
}

bool SerialDevice::send(std::span<const uint8_t> bytes)
{
    return m_link->write(bytes);
}

bool SerialDevice::send_framed(std::span<const uint8_t> message)
{
    std::vector<uint8_t> out;
    out.reserve(message.size() + 4);

    switch (m_spec.framing) {
    case SerialFraming::LINES:
        if (!P::encode_line(message, out)) {
            return false;
        }
        break;
    case SerialFraming::DELIMITED:
        if (!P::encode_delimited(message, m_spec.delimiter(), out)) {
            return false;
        }
        break;
    case SerialFraming::COBS:
        P::encode_cobs(message, out);
        break;
    case SerialFraming::SLIP:
        P::encode_slip(message, out);
        break;
    case SerialFraming::RAW:
    case SerialFraming::IDLE_GAP:
        out.assign(message.begin(), message.end());
        break;
    }

    return m_link->write(out);
}

bool SerialDevice::request(std::span<const uint8_t> command, ReplyMatcher matcher,
    std::chrono::milliseconds timeout, ReplyCallback done)
{
    if (!matcher || !done || m_link->state() != EndpointState::OPEN) {
        return false;
    }

    auto request = std::make_shared<Pending>();
    request->matcher = std::move(matcher);
    request->done = std::move(done);

    const bool installed = m_pending.update([&](std::shared_ptr<Pending>& slot) {
        if (slot) {
            return false;
        }
        slot = request;
        return true;
    });
    if (!installed) {
        return false;
    }

    request->timer.store(m_link->post_after(timeout, [this, request] {
        if (const auto expired = take_pending(request)) {
            end_request(expired, SerialReply::TIMED_OUT, {});
        }
    }));

    if (!is_pending(request)) {
        if (const auto token = request->timer.exchange(0); token != 0) {
            m_link->cancel_post(token);
        }
    }

    if (!m_link->write(command)) {
        if (const auto failed = take_pending(request)) {
            if (const auto token = failed->timer.exchange(0); token != 0) {
                m_link->cancel_post(token);
            }
            return false;
        }
        return true;
    }

    return true;
}

bool SerialDevice::set_control_lines(std::optional<bool> dtr, std::optional<bool> rts)
{
    return m_link->set_control_lines(dtr, rts);
}

bool SerialDevice::send_break()
{
    return m_link->send_break();
}

size_t SerialDevice::write_backlog() const
{
    return m_link->backlog();
}

SerialDeviceStats SerialDevice::stats() const
{
    return m_reader->stats();
}

void SerialDevice::handle_chunk(std::span<const uint8_t> chunk)
{
    m_reader->push(chunk);
    if (const auto readers = m_readers.read()) {
        for (const auto& entry : *readers) {
            entry.second->push(chunk);
        }
    }
}

void SerialDevice::handle_state(EndpointState previous, EndpointState current)
{
    m_reader->reset();
    if (const auto readers = m_readers.read()) {
        for (const auto& entry : *readers) {
            entry.second->reset();
        }
    }

    if (current != EndpointState::OPEN) {
        end_request(take_pending(), SerialReply::LINK_LOST, {});
    }

    if (m_on_state) {
        m_on_state(previous, current);
    }
}

bool SerialDevice::complete_request(P::Message frame)
{
    std::shared_ptr<Pending> request;
    if (const auto held = m_pending.read()) {
        request = *held;
    }
    if (!request || !request->matcher(frame)) {
        return false;
    }

    if (!take_pending(request)) {
        return false;
    }
    end_request(request, SerialReply::REPLIED, frame);
    return true;
}

bool SerialDevice::is_pending(const std::shared_ptr<Pending>& request) const
{
    const auto held = m_pending.read();
    return held && *held == request;
}

std::shared_ptr<SerialDevice::Pending> SerialDevice::take_pending(const std::shared_ptr<Pending>& only)
{
    std::shared_ptr<Pending> taken;
    m_pending.update([&](std::shared_ptr<Pending>& slot) {
        taken.reset();
        if (!slot || (only && slot != only)) {
            return false;
        }
        taken = std::move(slot);
        slot.reset();
        return true;
    });
    return taken;
}

void SerialDevice::end_request(const std::shared_ptr<Pending>& request, SerialReply how, P::Message reply)
{
    if (!request) {
        return;
    }
    if (const auto token = request->timer.exchange(0); token != 0) {
        m_link->cancel_post(token);
    }
    request->done(how, reply);
}

} // namespace MayaFlux::Core
