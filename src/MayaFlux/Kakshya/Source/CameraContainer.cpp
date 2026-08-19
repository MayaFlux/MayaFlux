#include "CameraContainer.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Processors/FrameAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/IOService.hpp"

namespace MayaFlux::Kakshya {

CameraContainer::CameraContainer(uint32_t width, uint32_t height,
    Portal::Graphics::ImageFormat format, double frame_rate)
    : VideoStreamContainer(width, height, format, frame_rate)
{
    m_num_frames = 1;

    m_data.resize(1);
    m_data[0] = make_empty_storage(format, get_frame_element_count());

    setup_dimensions();

    MF_INFO(Journal::Component::Kakshya, Journal::Context::Init,
        "CameraContainer created: {}x{} @{:.1f}fps ({} bytes/frame)",
        width, height, frame_rate, get_frame_byte_size());
}

uint8_t* CameraContainer::mutable_frame_ptr()
{
    if (m_data.empty())
        return nullptr;

    auto [ptr, bytes] = variant_bytes_mutable(m_data[0]);
    if (!ptr || bytes < get_frame_byte_size())
        return nullptr;

    return ptr;
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
