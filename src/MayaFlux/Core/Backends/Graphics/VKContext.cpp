#include "VKContext.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

bool VKContext::initialize(bool enable_validation,
    const std::vector<const char*>& required_extensions)
{
    if (!m_instance.initialize(enable_validation, required_extensions)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Failed to initialize Vulkan instance!");
        return false;
    }

    if (!m_device.initialize(m_instance.get_instance())) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend, "Failed to initialize Vulkan device!");
        cleanup();
        return false;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend, "Vulkan context initialized successfully.");
    return true;
}

void VKContext::cleanup()
{
    m_device.cleanup();
    m_instance.cleanup();
}

}
