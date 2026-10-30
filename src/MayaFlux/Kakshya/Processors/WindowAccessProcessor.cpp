#include "WindowAccessProcessor.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKEnumUtils.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"
#include "MayaFlux/Kakshya/Utils/SurfaceUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

namespace {

    bool surface_differs(const WindowContainer& container, uint32_t width, uint32_t height,
        WindowContainer::SurfaceFormat format)
    {
        const auto& structure = container.get_structure();
        return structure.get_width() != width
            || structure.get_height() != height
            || container.get_surface_format() != format;
    }

} // namespace

void WindowAccessProcessor::on_attach(
    const std::shared_ptr<SignalSourceContainer>& container)
{
    auto wc = std::dynamic_pointer_cast<WindowContainer>(container);
    if (!wc) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "WindowAccessProcessor requires a WindowContainer");
    }

    m_previous_window_capture_supported = wc->get_window()->is_capture_enabled();

    if (!m_previous_window_capture_supported) {
        wc->get_window()->set_capture_enabled(true);
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: capture was not enabled for '{}'; enabling now",
            wc->get_window()->get_create_info().title);
    }

    m_surface_format = query_surface_format(wc->get_window());

    const auto [surface_w, surface_h] = query_surface_extent(wc->get_window());
    if (surface_w > 0 && surface_h > 0 && surface_differs(*wc, surface_w, surface_h, m_surface_format))
        wc->handle_surface_resize(surface_w, surface_h, m_surface_format);

    const auto& s = std::as_const(*wc).get_structure();
    m_width = static_cast<uint32_t>(s.get_width());
    m_height = static_cast<uint32_t>(s.get_height());

    const uint32_t bpp = Core::vk_format_bytes_per_pixel(
        Core::to_vk_format(m_surface_format));

    container->mark_ready_for_processing(true);

    MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
        "WindowAccessProcessor attached: {}x{} format={} bpp={}",
        m_width, m_height,
        static_cast<uint32_t>(m_surface_format),
        bpp);
}

void WindowAccessProcessor::on_detach(
    const std::shared_ptr<SignalSourceContainer>& container)
{
    m_width = 0;
    m_height = 0;

    m_last_readback_bytes = 0;
    m_surface_format = Core::GraphicsSurfaceInfo::SurfaceFormat::B8G8R8A8_SRGB;

    if (!m_previous_window_capture_supported) {
        auto wc = std::dynamic_pointer_cast<WindowContainer>(container);
        if (wc) {
            wc->get_window()->set_capture_enabled(false);
            MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
                "WindowAccessProcessor detached: capture disabled for '{}', restoring previous state",
                wc->get_window()->get_create_info().title);
        }
    }
}

void WindowAccessProcessor::process(
    const std::shared_ptr<SignalSourceContainer>& container)
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
    const auto cur_format = query_surface_format(window);

    m_is_processing.store(true);

    DataVariant captured = std::vector<uint8_t> {};
    readback_region(window, 0U, 0U, cur_w, cur_h, captured);

    const bool has_data = std::visit(
        [](const auto& v) { return !v.empty(); }, captured);

    if (!has_data) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: readback returned no data for '{}'",
            window->get_create_info().title);
        m_is_processing.store(false);
        return;
    }

    container->update_processing_state(ProcessingState::PROCESSING);

    if (surface_differs(*wc, cur_w, cur_h, cur_format)) {
        m_width = cur_w;
        m_height = cur_h;
        wc->handle_surface_resize(cur_w, cur_h, cur_format);

        MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: '{}' resized to {}x{} format={}",
            window->get_create_info().title, m_width, m_height,
            static_cast<uint32_t>(cur_format));
    }

    m_surface_format = cur_format;

    if (!wc->publish_capture(std::move(captured))) {
        MF_RT_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "WindowAccessProcessor: capture does not match the history slots for '{}'; frame not kept",
            window->get_create_info().title);
    }

    const auto vk_fmt = Core::to_vk_format(m_surface_format);
    m_last_readback_bytes = static_cast<size_t>(m_width) * m_height
        * Core::vk_format_bytes_per_pixel(vk_fmt);

    m_is_processing.store(false);
    container->update_processing_state(ProcessingState::PROCESSED);
}

} // namespace MayaFlux::Kakshya
