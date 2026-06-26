#include "VisionProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"
#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"

#include "MayaFlux/Kriya/Awaiters/BroadcastAwaiter.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

namespace MayaFlux::Kakshya {

VisionProcessor::VisionProcessor(Kinesis::Vision::VisionSequence sequence)
    : m_sequence(std::move(sequence))
{
}

void VisionProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container)
        return;

    const bool valid = std::dynamic_pointer_cast<VideoStreamContainer>(container) || std::dynamic_pointer_cast<WindowContainer>(container) || std::dynamic_pointer_cast<TextureContainer>(container);

    if (!valid) {
        error<std::invalid_argument>(
            Journal::Component::Kakshya,
            Journal::Context::ContainerProcessing,
            std::source_location::current(),
            "VisionProcessor requires a VideoStreamContainer, WindowContainer, "
            "or TextureContainer; got {}",
            typeid(*container).name());
    }

    const auto& structure = container->get_structure();
    m_width = static_cast<uint32_t>(structure.get_width());
    m_height = static_cast<uint32_t>(structure.get_height());
    m_executor.reset();

    if (m_width == 0 || m_height == 0) {
        MF_WARN(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "VisionProcessor attached to container with zero spatial dimensions");
    } else {
        MF_INFO(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "VisionProcessor attached: {}x{}", m_width, m_height);
    }
}

void VisionProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& /*container*/)
{
    m_width = 0;
    m_height = 0;
    m_executor.reset();
}

void VisionProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (m_width == 0 || m_height == 0 || !container)
        return;

    std::span<const float> frame;

    if (auto vc = std::dynamic_pointer_cast<VideoStreamContainer>(container)) {
        frame = vc->processed_frame_as_float(0);
        vc->invalidate_float_frame_cache(0);
    } else if (auto wc = std::dynamic_pointer_cast<WindowContainer>(container)) {
        frame = wc->processed_frame_as_float(0);
        wc->invalidate_float_frame_cache(0);
    } else if (auto tc = std::dynamic_pointer_cast<TextureContainer>(container)) {
        frame = tc->as_normalised_float(0);
    }

    if (frame.empty())
        return;

    m_is_processing.store(true, std::memory_order_release);

    m_result = m_executor.run(m_sequence, frame, m_width, m_height);
    if (m_result_source)
        m_result_source->signal(m_result);

    m_is_processing.store(false, std::memory_order_release);
}

void VisionProcessor::set_sequence(Kinesis::Vision::VisionSequence sequence)
{
    m_sequence = std::move(sequence);
    m_executor.reset();
}

std::shared_ptr<Vruta::BroadcastSource<Kinesis::Vision::VisionResult>> VisionProcessor::get_result_source()
{
    if (!m_result_source)
        m_result_source = std::make_shared<Vruta::BroadcastSource<Kinesis::Vision::VisionResult>>();
    return m_result_source;
}
} // namespace MayaFlux::Kakshya
