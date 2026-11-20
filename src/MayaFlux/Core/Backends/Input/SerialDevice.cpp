#include "SerialDevice.hpp"

#include "MayaFlux/Transitive/Reflect/TextCodec.hpp"

namespace MayaFlux::Core {

namespace P = Transitive::Protocol;

namespace {

    std::optional<InputValue> parse_numbers_of(P::Message frame, uint32_t device_id)
    {
        std::string text(P::as_text(frame));
        std::ranges::replace_if(text, [](char c) { return c == ',' || c == ';'; }, ' ');

        auto numbers = Reflect::parse_numbers<double>(text);
        if (!numbers || numbers->empty()) {
            return std::nullopt;
        }
        return InputValue::make_vector(std::move(*numbers), device_id, InputType::SERIAL);
    }

}

SerialDevice::SerialDevice(uint32_t device_id, SerialSpec spec, std::unique_ptr<ISerialLink> link)
    : m_device_id(device_id)
    , m_spec(std::move(spec))
    , m_link(std::move(link))
    , m_framer(make_framer(m_spec))
{
    m_link->set_chunk_callback([this](std::span<const uint8_t> chunk) { handle_chunk(chunk); });
    m_link->set_state_callback([this](EndpointState previous, EndpointState current) {
        handle_state(previous, current);
    });
}

SerialDevice::~SerialDevice()
{
    m_link->close();
    cancel_gap();
    if (const auto dropped = take_pending()) {
        if (const auto token = dropped->timer.exchange(0); token != 0) {
            m_link->cancel_post(token);
        }
    }
}

auto SerialDevice::make_framer(const SerialSpec& spec) -> Framer
{
    const size_t limit = spec.message_limit;

    switch (spec.framing) {
    case SerialFraming::LINES:
        return Framer { std::in_place_type<P::LineFramer>, limit };
    case SerialFraming::DELIMITED: {
        const auto terminator = spec.delimiter();
        return Framer { std::in_place_type<P::DelimiterFramer>, std::span<const uint8_t> { terminator }, limit };
    }
    case SerialFraming::COBS:
        return Framer { std::in_place_type<P::CobsFramer>, limit };
    case SerialFraming::SLIP:
        return Framer { std::in_place_type<P::SlipFramer>, limit };
    case SerialFraming::IDLE_GAP:
        return Framer { std::in_place_type<P::IdleGapFramer>, std::chrono::duration_cast<std::chrono::microseconds>(spec.gap), limit };
    case SerialFraming::RAW:
        break;
    }
    return Framer { std::in_place_type<P::RawChunkFramer> };
}

void SerialDevice::set_value_callback(ValueCallback callback)
{
    m_on_value = std::move(callback);
}

void SerialDevice::set_chunk_callback(ChunkCallback callback)
{
    m_on_chunk = std::move(callback);
}

void SerialDevice::set_state_callback(StateCallback callback)
{
    m_on_state = std::move(callback);
}

bool SerialDevice::open(const std::string& path)
{
    reset_stream();
    return m_link->open(path, m_spec.line);
}

void SerialDevice::close()
{
    m_link->close();
    cancel_gap();
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

    if (!m_link->write(command)) {
        if (const auto failed = take_pending(request)) {
            m_link->cancel_post(failed->timer.exchange(0));
        }
        return false;
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
    return { .frames = m_frames.load(), .dropped = m_dropped.load(), .unparsed = m_unparsed.load() };
}

void SerialDevice::handle_chunk(std::span<const uint8_t> chunk)
{
    if (m_on_chunk) {
        m_on_chunk(chunk);
        return;
    }

    if (m_spec.parse == SerialParse::MIDI) {
        parse_midi(chunk);
        return;
    }

    const auto on_frame = [this](P::Message frame) { handle_frame(frame); };
    std::visit(
        [&](auto& framer) {
            const auto report = framer.push(chunk, on_frame);
            m_dropped.fetch_add(report.discarded);
            if constexpr (std::is_same_v<std::decay_t<decltype(framer)>, P::IdleGapFramer>) {
                arm_gap();
            }
        },
        m_framer);
}

void SerialDevice::handle_frame(P::Message frame)
{
    m_frames.fetch_add(1);

    if (complete_request(frame)) {
        return;
    }

    switch (m_spec.parse) {
    case SerialParse::BYTES:
        deliver(InputValue::make_bytes(std::vector<uint8_t>(frame.begin(), frame.end()), m_device_id, InputType::SERIAL));
        break;
    case SerialParse::NUMBERS:
        deliver(parse_numbers_of(frame, m_device_id));
        break;
    case SerialParse::CUSTOM:
        deliver(m_spec.parser ? m_spec.parser(frame) : std::nullopt);
        break;
    case SerialParse::MIDI:
        break;
    }
}

void SerialDevice::deliver(std::optional<InputValue> value)
{
    if (!value) {
        m_unparsed.fetch_add(1);
        return;
    }

    value->device_id = m_device_id;
    value->source_type = InputType::SERIAL;
    if (m_on_value) {
        m_on_value(*value);
    }
}

void SerialDevice::parse_midi(std::span<const uint8_t> chunk)
{
    const auto emit = [this](uint8_t status, uint8_t first, uint8_t second) {
        m_frames.fetch_add(1);
        deliver(InputValue::make_midi(status, first, second, m_device_id));
    };

    for (const uint8_t byte : chunk) {
        if (byte >= 0xF8) {
            emit(byte, 0, 0);
            continue;
        }

        if ((byte & 0x80) != 0) {
            m_midi.have = 0;
            if (byte >= 0xF0) {
                m_midi.status = 0;
                m_midi.need = 0;
                m_midi.in_sysex = byte == 0xF0;
                continue;
            }
            m_midi.in_sysex = false;
            m_midi.status = byte;
            const uint8_t kind = byte & 0xF0;
            m_midi.need = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
            continue;
        }

        if (m_midi.in_sysex || m_midi.status == 0) {
            continue;
        }

        m_midi.data[m_midi.have++] = byte;
        if (m_midi.have == m_midi.need) {
            emit(m_midi.status, m_midi.data[0], m_midi.need == 2 ? m_midi.data[1] : uint8_t { 0 });
            m_midi.have = 0;
        }
    }
}

void SerialDevice::arm_gap()
{
    cancel_gap();
    const auto delay = std::max(m_spec.gap, std::chrono::milliseconds(1));
    m_gap_timer.store(m_link->post_after(delay, [this] { handle_gap(); }));
}

void SerialDevice::cancel_gap()
{
    if (const auto token = m_gap_timer.exchange(0); token != 0) {
        m_link->cancel_post(token);
    }
}

void SerialDevice::handle_gap()
{
    m_gap_timer.store(0);
    const auto on_frame = [this](P::Message frame) { handle_frame(frame); };
    if (auto* framer = std::get_if<P::IdleGapFramer>(&m_framer)) {
        const auto report = framer->flush(on_frame);
        m_dropped.fetch_add(report.discarded);
    }
}

void SerialDevice::handle_state(EndpointState previous, EndpointState current)
{
    reset_stream();

    if (current != EndpointState::OPEN) {
        end_request(take_pending(), SerialReply::LINK_LOST, {});
    }

    if (m_on_state) {
        m_on_state(previous, current);
    }
}

void SerialDevice::reset_stream()
{
    cancel_gap();
    std::visit([](auto& framer) { framer.reset(); }, m_framer);
    m_midi = {};
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
