#include "WindowAccessProcessor.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"
#include "MayaFlux/Kakshya/Utils/SurfaceUtils.hpp"

namespace MayaFlux::Kakshya {

void WindowAccessProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto wc = std::dynamic_pointer_cast<WindowContainer>(container);
    if (!wc) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "WindowAccessProcessor requires a WindowContainer");
    }

    const auto& s = wc->get_structure();
    m_width = static_cast<uint32_t>(s.get_width());
    m_height = static_cast<uint32_t>(s.get_height());
    m_channels = static_cast<uint32_t>(s.get_channel_count());

    container->mark_ready_for_processing(true);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowAccessProcessor attached: {}x{} {} ch",
        m_width, m_height, m_channels);
}

void WindowAccessProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& /*container*/)
{
    m_width = 0;
    m_height = 0;
    m_channels = 0;
    m_last_readback_bytes = 0;
}

void WindowAccessProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto wc = std::dynamic_pointer_cast<WindowContainer>(container);
    if (!wc) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor::process — container is not a WindowContainer");
        return;
    }

    const auto& window = wc->get_window();

    if (!is_readback_available(window)) {
        MF_RT_TRACE(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: no completed frame available for '{}'",
            window->get_create_info().title);
        return;
    }

    const auto [cur_w, cur_h] = query_surface_extent(window);
    if (cur_w != m_width || cur_h != m_height) {
        m_width = cur_w;
        m_height = cur_h;

        MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: '{}' resized to {}x{}",
            window->get_create_info().title, m_width, m_height);
    }

    m_is_processing.store(true);
    container->update_processing_state(ProcessingState::PROCESSING);

    auto& processed = container->get_processed_data();
    processed.resize(1);

    DataAccess result = readback_region(
        window, 0U, 0U, m_width, m_height, m_channels, processed[0]);

    if (result.element_count() == 0) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: full-surface readback returned no data for '{}'",
            window->get_create_info().title);
        m_is_processing.store(false);
        container->update_processing_state(ProcessingState::ERROR);
        return;
    }

    m_last_readback_bytes = static_cast<size_t>(m_width) * m_height * m_channels;
    m_is_processing.store(false);
    container->update_processing_state(ProcessingState::PROCESSED);
}

} // namespace MayaFlux::Kakshya
