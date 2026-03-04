#include "CameraContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Processors/FrameAccessProcessor.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/IOService.hpp"

namespace MayaFlux::Kakshya {

CameraContainer::CameraContainer(uint32_t width, uint32_t height,
    uint32_t channels, double frame_rate)
    : VideoStreamContainer(width, height, channels, frame_rate)
{
    m_num_frames = 1;

    const size_t frame_bytes = get_frame_byte_size();

    m_data.resize(1);
    auto& pixels = m_data[0].emplace<std::vector<uint8_t>>();
    pixels.resize(frame_bytes, 0);

    setup_dimensions();

    MF_INFO(Journal::Component::Kakshya, Journal::Context::Init,
        "CameraContainer created: {}x{} @{:.1f}fps ({} bytes/frame)",
        width, height, frame_rate, frame_bytes);
}

uint8_t* CameraContainer::mutable_frame_ptr()
{
    if (m_data.empty())
        return nullptr;

    auto* pixels = std::get_if<std::vector<uint8_t>>(&m_data[0]);
    if (!pixels || pixels->empty())
        return nullptr;

    return pixels->data();
}

void CameraContainer::setup_io(uint64_t reader_id)
{
    m_io_reader_id = reader_id;
    m_io_service = Registry::BackendRegistry::instance()
                       .get_service<Registry::Service::IOService>();

    MF_DEBUG(Journal::Component::Kakshya, Journal::Context::Init,
        "CameraContainer: wired IOService with reader_id={}", reader_id);
}

void CameraContainer::create_default_processor()
{
    auto processor = std::make_shared<FrameAccessProcessor>();
    processor->set_auto_advance(false);
    processor->set_global_fps(m_frame_rate);
    set_default_processor(processor);
}

void CameraContainer::process_default()
{
    VideoStreamContainer::process_default();

    if (m_io_service && m_io_service->request_frame)
        m_io_service->request_frame(m_io_reader_id);
}

} // namespace MayaFlux::Kakshya
