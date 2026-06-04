#include "SDFPrepProcessor.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

SDFPrepProcessor::SDFPrepProcessor(uint32_t res_x, uint32_t res_y, uint32_t res_z)
    : m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

void SDFPrepProcessor::set_resolution(uint32_t res_x, uint32_t res_y, uint32_t res_z)
{
    m_res_x = std::max(res_x, 1U);
    m_res_y = std::max(res_y, 1U);
    m_res_z = std::max(res_z, 1U);
    rebuild_buffers();
}

void SDFPrepProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ensure_initialized(std::dynamic_pointer_cast<VKBuffer>(buffer));
    rebuild_buffers();
}

void SDFPrepProcessor::processing_function(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (!m_grid_buf || !m_counter_buf)
        return;

    std::memset(m_grid_buf->get_mapped_ptr(), 0, corner_count() * sizeof(float));
    std::memset(m_counter_buf->get_mapped_ptr(), 0, sizeof(uint32_t));
}

void SDFPrepProcessor::rebuild_buffers()
{
    auto svc = Registry::BackendRegistry::instance()
                   .get_service<Registry::Service::BufferService>();

    m_grid_buf = std::make_shared<VKBuffer>(
        corner_count() * sizeof(float),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_grid_buf);

    m_counter_buf = std::make_shared<VKBuffer>(
        sizeof(uint32_t),
        VKBuffer::Usage::HOST_STORAGE,
        Kakshya::DataModality::UNKNOWN);
    svc->initialize_buffer(m_counter_buf);
}

} // namespace MayaFlux::Buffers
