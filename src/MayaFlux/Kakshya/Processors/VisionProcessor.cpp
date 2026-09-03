#include "VisionProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"
#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"

#include "MayaFlux/Kriya/Awaiters/BroadcastAwaiter.hpp"
#include "MayaFlux/Vruta/BroadcastSource.hpp"

namespace MayaFlux::Kakshya {

VisionProcessor::VisionProcessor(Kinesis::Vision::VisionSequence sequence, bool force_cpu)
    : m_sequence(std::move(sequence))
    , m_force_cpu(force_cpu)
{
    if (!m_force_cpu)
        m_executor = std::make_unique<Yantra::VisionGpuExecutor>();
}

void VisionProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (!container)
        return;

    const bool valid = std::dynamic_pointer_cast<VideoStreamContainer>(container)
        || std::dynamic_pointer_cast<WindowContainer>(container)
        || std::dynamic_pointer_cast<TextureContainer>(container);

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

    if (m_force_cpu) {
        m_cpu_executor.reset();
    } else if (m_width > 0 && m_height > 0) {
        if (!m_executor)
            m_executor = std::make_unique<Yantra::VisionGpuExecutor>();

        auto& loom = Portal::Graphics::TextureLoom::instance();
        m_gpu_frame = loom.create_2d(m_width, m_height, Portal::Graphics::ImageFormat::RGBA8, nullptr);
        m_upload_staging = Buffers::create_image_staging_buffer(m_gpu_frame->get_size_bytes());
    }

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

    if (m_force_cpu)
        m_cpu_executor.reset();

    m_gpu_frame.reset();
}

void VisionProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    if (m_width == 0 || m_height == 0 || !container)
        return;

    m_is_processing.store(true, std::memory_order_release);

    if (m_force_cpu) {
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

        if (!frame.empty())
            m_result = m_cpu_executor.run(m_sequence, frame, m_width, m_height);
    } else if (m_gpu_frame) {
        if (m_executor->is_suspended()) {
            auto poll = m_executor->run(m_sequence, m_gpu_frame, m_width, m_height);
            if (!poll.is_ready()) {
                m_is_processing.store(false, std::memory_order_release);
                return;
            }
            m_result = std::move(poll);
        } else if (const void* raw = container->get_raw_data()) {
            auto& loom = Portal::Graphics::TextureLoom::instance();
            loom.upload_data(m_gpu_frame, raw, m_gpu_frame->get_size_bytes(), m_upload_staging);

            auto pass = m_executor->run(m_sequence, m_gpu_frame, m_width, m_height);
            if (!pass.is_ready()) {
                m_is_processing.store(false, std::memory_order_release);
                return;
            }
            m_result = std::move(pass);
        }
    }

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
