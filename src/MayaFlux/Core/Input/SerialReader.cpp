#include "SerialReader.hpp"

#include "MayaFlux/Transitive/Reflect/TextCodec.hpp"

namespace MayaFlux::Core {

namespace P = Transitive::Protocol;

namespace {

    std::optional<InputValue> parse_numbers_of(P::Message frame, uint32_t id)
    {
        std::string text(P::as_text(frame));
        std::ranges::replace_if(text, [](char c) { return c == ',' || c == ';'; }, ' ');

        auto numbers = Reflect::parse_numbers<double>(text);
        if (!numbers || numbers->empty()) {
            return std::nullopt;
        }
        return InputValue::make_vector(std::move(*numbers), id, InputType::SERIAL);
    }

}

SerialReader::SerialReader(uint32_t id, SerialSpec spec, ISerialLink& link)
    : m_id(id)
    , m_spec(std::move(spec))
    , m_link(link)
    , m_framer(make_framer(m_spec))
{
}

SerialReader::~SerialReader()
{
    cancel_gap();
}

auto SerialReader::make_framer(const SerialSpec& spec) -> Framer
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

void SerialReader::set_value_callback(ValueCallback callback)
{
    m_on_value = std::move(callback);
}

void SerialReader::set_frame_filter(FrameFilter filter)
{
    m_filter = std::move(filter);
}

SerialReaderStats SerialReader::stats() const
{
    return { .frames = m_frames.load(), .dropped = m_dropped.load(), .unparsed = m_unparsed.load() };
}

void SerialReader::push(std::span<const uint8_t> chunk)
{
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

void SerialReader::reset()
{
    cancel_gap();
    std::visit([](auto& framer) { framer.reset(); }, m_framer);
    m_midi = {};
}

void SerialReader::handle_frame(P::Message frame)
{
    m_frames.fetch_add(1);

    if (m_filter && m_filter(frame)) {
        return;
    }

    switch (m_spec.parse) {
    case SerialParse::BYTES:
        deliver(InputValue::make_bytes(std::vector<uint8_t>(frame.begin(), frame.end()), m_id, InputType::SERIAL));
        break;
    case SerialParse::NUMBERS:
        deliver(parse_numbers_of(frame, m_id));
        break;
    case SerialParse::CUSTOM:
        deliver(m_spec.parser ? m_spec.parser(frame) : std::nullopt);
        break;
    case SerialParse::MIDI:
        break;
    }
}

void SerialReader::deliver(std::optional<InputValue> value)
{
    if (!value) {
        m_unparsed.fetch_add(1);
        return;
    }

    value->device_id = m_id;
    value->source_type = InputType::SERIAL;
    if (m_on_value) {
        m_on_value(*value);
    }
}

void SerialReader::parse_midi(std::span<const uint8_t> chunk)
{
    const auto emit = [this](uint8_t status, uint8_t first, uint8_t second) {
        m_frames.fetch_add(1);
        deliver(InputValue::make_midi(status, first, second, m_id));
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

void SerialReader::arm_gap()
{
    cancel_gap();
    const auto delay = std::max(m_spec.gap, std::chrono::milliseconds(1));
    m_gap_timer.store(m_link.post_after(delay, [this] { handle_gap(); }));
}

void SerialReader::cancel_gap()
{
    if (const auto token = m_gap_timer.exchange(0); token != 0) {
        m_link.cancel_post(token);
    }
}

void SerialReader::handle_gap()
{
    m_gap_timer.store(0);
    const auto on_frame = [this](P::Message frame) { handle_frame(frame); };
    if (auto* framer = std::get_if<P::IdleGapFramer>(&m_framer)) {
        const auto report = framer->flush(on_frame);
        m_dropped.fetch_add(report.discarded);
    }
}

} // namespace MayaFlux::Core
