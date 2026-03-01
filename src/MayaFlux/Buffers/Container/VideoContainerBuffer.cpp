#include "VideoContainerBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Textures/TextureProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

VideoStreamReader::VideoStreamReader(const std::shared_ptr<Kakshya::StreamContainer>& container)
    : m_container(container)
{
    if (container) {
        container->register_state_change_callback(
            [this](const auto& c, auto s) {
                this->on_container_state_change(c, s);
            });
    }
}

// =========================================================================
// Lifecycle
// =========================================================================

void VideoStreamReader::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    m_reader_id = m_container->register_dimension_reader(0);

    if (!m_container->is_ready_for_processing()) {
        error<std::runtime_error>(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            std::source_location::current(),
            "VideoStreamReader: Container not ready for processing");
    }

    try {
        auto texture_buffer = std::dynamic_pointer_cast<TextureBuffer>(buffer);
        if (!texture_buffer) {
            error<std::runtime_error>(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                std::source_location::current(),
                "VideoStreamReader: Buffer must be a TextureBuffer");
        }

        extract_frame_data(texture_buffer);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

    } catch (const std::exception& e) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VideoStreamReader: Error during on_attach: {}", e.what());
    }
}

void VideoStreamReader::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_container) {
        m_container->unregister_dimension_reader(0);
        m_container->unregister_state_change_callback();
    }
}

// =========================================================================
// Processing
// =========================================================================

void VideoStreamReader::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    if (m_container->is_at_end()) {
        buffer->mark_for_removal();
        return;
    }

    try {
        auto state = m_container->get_processing_state();

        if (state == Kakshya::ProcessingState::NEEDS_REMOVAL) {
            if (m_update_flags) {
                buffer->mark_for_removal();
            }
            return;
        }

        if (state != Kakshya::ProcessingState::PROCESSED) {
            if (state == Kakshya::ProcessingState::READY) {
                if (m_container->try_acquire_processing_token(0)) {
                    m_container->process_default();
                }
            }
        }

        auto texture_buffer = std::dynamic_pointer_cast<TextureBuffer>(buffer);
        if (!texture_buffer) {
            return;
        }

        extract_frame_data(texture_buffer);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

        m_container->mark_dimension_consumed(0, m_reader_id);

        if (m_container->all_dimensions_consumed()) {
            m_container->update_processing_state(Kakshya::ProcessingState::READY);
            m_container->reset_processing_token();
        }

    } catch (const std::exception& e) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VideoStreamReader: Error during processing: {}", e.what());
    }
}

// =========================================================================
// Frame extraction
// =========================================================================

void VideoStreamReader::extract_frame_data(const std::shared_ptr<TextureBuffer>& texture_buffer)
{
    if (!m_container) {
        return;
    }

    auto& processed_data = m_container->get_processed_data();
    if (processed_data.empty()) {
        return;
    }

    const auto* pixel_vec = std::get_if<std::vector<uint8_t>>(&processed_data[0]);
    if (!pixel_vec || pixel_vec->empty()) {
        return;
    }

    uint64_t expected_size = static_cast<uint64_t>(texture_buffer->get_width())
        * texture_buffer->get_height() * 4;

    size_t copy_size = std::min(pixel_vec->size(), static_cast<size_t>(expected_size));

    texture_buffer->set_pixel_data(pixel_vec->data(), copy_size);
}

// =========================================================================
// Container management
// =========================================================================

void VideoStreamReader::set_container(const std::shared_ptr<Kakshya::StreamContainer>& container)
{
    if (m_container) {
        m_container->unregister_dimension_reader(0);
        m_container->unregister_state_change_callback();
    }

    m_container = container;

    if (container) {
        m_reader_id = container->register_dimension_reader(0);
        container->register_state_change_callback(
            [this](const auto& c, auto s) {
                this->on_container_state_change(c, s);
            });
    }
}

// =========================================================================
// State callback
// =========================================================================

void VideoStreamReader::on_container_state_change(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& /*container*/,
    Kakshya::ProcessingState state)
{
    switch (state) {
    case Kakshya::ProcessingState::NEEDS_REMOVAL:
        MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VideoStreamReader: Container marked for removal");
        break;

    case Kakshya::ProcessingState::ERROR:
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VideoStreamReader: Container entered ERROR state");
        break;

    default:
        break;
    }
}

// =========================================================================
// VideoContainerBuffer implementation
// =========================================================================

VideoContainerBuffer::VideoContainerBuffer(
    const std::shared_ptr<Kakshya::StreamContainer>& container,
    Portal::Graphics::ImageFormat format)
    : TextureBuffer(
          static_cast<uint32_t>(container->get_structure().get_width()),
          static_cast<uint32_t>(container->get_structure().get_height()),
          format)
    , m_container(container)
{
    if (!m_container) {
        error<std::invalid_argument>(Journal::Component::Buffers, Journal::Context::Init,
            std::source_location::current(),
            "VideoContainerBuffer: container must not be null");
    }

    m_video_reader = std::make_shared<VideoStreamReader>(m_container);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VideoContainerBuffer created: {}x{} from container",
        get_width(), get_height());
}

void VideoContainerBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<VideoContainerBuffer>(shared_from_this());

    m_video_reader = std::make_shared<VideoStreamReader>(m_container);
    m_video_reader->set_processing_token(token);
    set_default_processor(m_video_reader);
    enforce_default_processing(true);

    auto texture_proc = std::make_shared<TextureProcessor>();
    texture_proc->set_streaming_mode(true);
    texture_proc->set_processing_token(token);
    set_texture_processor(texture_proc);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);
    chain->add_preprocessor(texture_proc, self);

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "VideoContainerBuffer setup_processors: VideoStreamReader as default, "
        "TextureProcessor as preprocessor");
}

void VideoContainerBuffer::set_container(const std::shared_ptr<Kakshya::StreamContainer>& container)
{
    m_container = container;

    if (m_video_reader) {
        m_video_reader->set_container(container);
    }
}

} // namespace MayaFlux::Buffers
