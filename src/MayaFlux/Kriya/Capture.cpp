#include "Capture.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "Bridge.hpp"

namespace MayaFlux::Kriya {

BufferCapture::BufferCapture(std::shared_ptr<Buffers::AudioBuffer> buffer,
    CaptureMode mode,
    uint32_t cycle_count)
    : m_buffer(std::move(buffer))
    , m_mode(mode)
    , m_cycle_count(cycle_count)
    , m_window_size(0)
    , m_circular_size(0)
    , m_overlap_ratio(0.0F)
{
}

BufferCapture& BufferCapture::with_processing_control(ProcessingControl control)
{
    m_processing_control = control;
    return *this;
}

BufferCapture& BufferCapture::for_cycles(uint32_t count)
{
    m_cycle_count = count;
    m_mode = (count > 1) ? CaptureMode::ACCUMULATE : CaptureMode::TRANSIENT;
    return *this;
}

BufferCapture& BufferCapture::until_condition(std::function<bool()> predicate)
{
    m_stop_condition = std::move(predicate);
    m_mode = CaptureMode::TRIGGERED;
    return *this;
}

BufferCapture& BufferCapture::with_window(uint32_t window_size, float overlap_ratio)
{
    m_window_size = window_size;
    m_overlap_ratio = overlap_ratio;
    m_mode = CaptureMode::WINDOWED;
    return *this;
}

BufferCapture& BufferCapture::as_circular(uint32_t buffer_size)
{
    m_circular_size = buffer_size;
    m_mode = CaptureMode::CIRCULAR;
    return *this;
}

BufferCapture& BufferCapture::on_data_ready(OperationFunction callback)
{
    m_data_ready_callback = std::move(callback);
    return *this;
}

BufferCapture& BufferCapture::on_cycle_complete(std::function<void(uint32_t)> callback)
{
    m_cycle_callback = std::move(callback);
    return *this;
}

BufferCapture& BufferCapture::on_data_expired(std::function<void(const Kakshya::DataVariant&, uint32_t)> callback)
{
    m_data_expired_callback = std::move(callback);
    return *this;
}

BufferCapture& BufferCapture::with_tag(const std::string& tag)
{
    m_tag = tag;
    return *this;
}

BufferCapture& BufferCapture::with_metadata(const std::string& key, const std::string& value)
{
    m_metadata[key] = value;
    return *this;
}

CaptureBuilder::CaptureBuilder(std::shared_ptr<Buffers::AudioBuffer> buffer)
    : m_capture(std::move(buffer))
{
}

CaptureBuilder& CaptureBuilder::on_capture_processing()
{
    m_capture.with_processing_control(BufferCapture::ProcessingControl::ON_CAPTURE);
    return *this;
}

CaptureBuilder& CaptureBuilder::manual_processing()
{
    m_capture.with_processing_control(BufferCapture::ProcessingControl::MANUAL);
    return *this;
}

CaptureBuilder& CaptureBuilder::auto_processing()
{
    m_capture.with_processing_control(BufferCapture::ProcessingControl::AUTOMATIC);
    return *this;
}

CaptureBuilder& CaptureBuilder::for_cycles(uint32_t count)
{
    m_capture.for_cycles(count);
    return *this;
}

CaptureBuilder& CaptureBuilder::until_condition(std::function<bool()> predicate)
{
    m_capture.until_condition(std::move(predicate));
    return *this;
}

CaptureBuilder& CaptureBuilder::as_circular(uint32_t buffer_size)
{
    m_capture.as_circular(buffer_size);
    return *this;
}

CaptureBuilder& CaptureBuilder::with_window(uint32_t window_size, float overlap_ratio)
{
    m_capture.with_window(window_size, overlap_ratio);
    return *this;
}

CaptureBuilder& CaptureBuilder::on_data_ready(OperationFunction callback)
{
    m_capture.on_data_ready(std::move(callback));
    return *this;
}

CaptureBuilder& CaptureBuilder::on_cycle_complete(std::function<void(uint32_t)> callback)
{
    m_capture.on_cycle_complete(std::move(callback));
    return *this;
}

CaptureBuilder& CaptureBuilder::on_data_expired(std::function<void(const Kakshya::DataVariant&, uint32_t)> callback)
{
    m_capture.on_data_expired(std::move(callback));
    return *this;
}

CaptureBuilder& CaptureBuilder::with_tag(const std::string& tag)
{
    m_capture.with_tag(tag);
    return *this;
}

CaptureBuilder& CaptureBuilder::with_metadata(const std::string& key, const std::string& value)
{
    m_capture.with_metadata(key, value);
    return *this;
}

CaptureBuilder::operator BufferOperation()
{
    return { BufferOperation::OpType::CAPTURE, std::move(m_capture) };
}

}
