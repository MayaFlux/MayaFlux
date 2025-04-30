#include "ContainerBuffer.hpp"
#include "MayaFlux/Kakshya/SignalSourceContainer.hpp"

namespace MayaFlux::Buffers {

ContainerToBufferAdapter::ContainerToBufferAdapter(std::shared_ptr<Kakshya::SignalSourceContainer> container)
    : m_container(container)
    , m_source_channel(0)
    , m_auto_advance(true)
    , m_update_flags(true)
{
    if (container) {
        try {
            container->register_state_change_callback(
                [this](std::shared_ptr<Kakshya::SignalSourceContainer> c, Kakshya::ProcessingState s) {
                    this->on_container_state_change(c, s);
                });
        } catch (const std::bad_weak_ptr& e) {
            std::cerr << "Warning: Could not register callback during adapter construction: "
                      << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Unexpected exception in adapter construction: "
                      << e.what() << std::endl;
        }
    }
}

void ContainerToBufferAdapter::process(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    try {
        m_container->lock();

        auto state = m_container->get_processing_state();

        if (state == Kakshya::ProcessingState::NEEDS_REMOVAL) {
            if (m_update_flags) {
                buffer->mark_for_removal();
            }
            m_container->unlock();
            return;
        }

        if (state == Kakshya::ProcessingState::PROCESSED) {
            const auto& processed_data = m_container->get_processed_data();

            if (m_source_channel < processed_data.size()) {
                const auto& channel_data = processed_data[m_source_channel];
                std::vector<double>& buffer_data = buffer->get_data();

                u_int32_t copy_samples = std::min(
                    static_cast<u_int32_t>(channel_data.size()),
                    buffer->get_num_samples());

                if (buffer_data.size() < copy_samples) {
                    buffer_data.resize(copy_samples);
                }

                std::copy(channel_data.begin(),
                    channel_data.begin() + copy_samples,
                    buffer_data.begin());

                if (copy_samples < buffer_data.size()) {
                    std::fill(buffer_data.begin() + copy_samples,
                        buffer_data.end(), 0.0);
                }

                if (m_update_flags) {
                    buffer->mark_for_processing(true);
                }
            }

            if (m_auto_advance) {
                m_container->mark_channel_consumed(m_source_channel);

                if (m_container->all_channels_consumed()) {
                    m_container->update_processing_state(Kakshya::ProcessingState::READY);
                }
            }
        }

        m_container->unlock();

    } catch (const std::exception& e) {
        std::cerr << "Error in ContainerToBufferAdapter::process: " << e.what() << std::endl;
        m_container->unlock();
    }
}

void ContainerToBufferAdapter::on_attach(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    if (!m_container || !buffer) {
        return;
    }

    m_container->register_channel_reader(m_source_channel);

    if (!m_container->is_ready_for_processing()) {
        throw std::invalid_argument("Supplied container is not prepared for evaluation! Aborting");
    }

    try {
        m_container->lock();

        auto& buffer_data = buffer->get_data();
        u_int32_t num_samples = buffer->get_num_samples();

        u_int64_t position = m_container->get_read_position();

        m_container->fill_sample_range(position, num_samples, buffer_data, m_source_channel);

        if (m_update_flags) {
            buffer->mark_for_processing(true);
        }

        m_container->unlock();

    } catch (const std::exception& e) {
        std::cerr << std::format("Error in ContainerToBufferAdapter::on_attach: {}", e.what()) << std::endl;
        m_container->unlock();
    }
}

void ContainerToBufferAdapter::on_detach(std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
        m_container->unregister_channel_reader(m_source_channel);
    }
}

void ContainerToBufferAdapter::set_source_channel(u_int32_t channel)
{
    m_source_channel = channel;
}

u_int32_t ContainerToBufferAdapter::get_source_channel() const
{
    return m_source_channel;
}

void ContainerToBufferAdapter::set_container(std::shared_ptr<Kakshya::SignalSourceContainer> container)
{
    if (m_container) {
        m_container->unregister_state_change_callback();
    }

    m_container = container;

    if (container) {
        container->register_state_change_callback(
            [this](std::shared_ptr<Kakshya::SignalSourceContainer> c, Kakshya::ProcessingState s) {
                this->on_container_state_change(c, s);
            });
    }
}

std::shared_ptr<Kakshya::SignalSourceContainer> ContainerToBufferAdapter::get_container() const
{
    return m_container;
}

void ContainerToBufferAdapter::set_auto_advance(bool enable)
{
    m_auto_advance = enable;
}

bool ContainerToBufferAdapter::get_auto_advance() const
{
    return m_auto_advance;
}

void ContainerToBufferAdapter::set_update_flags(bool update)
{
    m_update_flags = update;
}

bool ContainerToBufferAdapter::get_update_flags() const
{
    return m_update_flags;
}

void ContainerToBufferAdapter::on_container_state_change(
    std::shared_ptr<Kakshya::SignalSourceContainer> container,
    Kakshya::ProcessingState state)
{
    if (container != m_container) {
        return;
    }

    switch (state) {
    case Kakshya::ProcessingState::NEEDS_REMOVAL:
        break;

    case Kakshya::ProcessingState::READY:
        break;

    case Kakshya::ProcessingState::PROCESSING:
        break;

    case Kakshya::ProcessingState::PROCESSED:
        break;

    case Kakshya::ProcessingState::IDLE:
        break;
    }
}

ContainerBuffer::ContainerBuffer(u_int32_t channel_id, u_int32_t num_samples,
    std::shared_ptr<Kakshya::SignalSourceContainer> container,
    u_int32_t source_channel)
    : StandardAudioBuffer(channel_id, num_samples)
    , m_container(container)
    , m_source_channel(source_channel)
    , m_pending_adapter(nullptr)
{
    if (!m_container) {
        throw std::invalid_argument("ContainerBuffer: container must not be null");
    }

    m_pending_adapter = std::make_shared<ContainerToBufferAdapter>(m_container);
    std::dynamic_pointer_cast<ContainerToBufferAdapter>(m_pending_adapter)->set_source_channel(m_source_channel);
}

void ContainerBuffer::initialize()
{
    if (m_pending_adapter) {
        set_default_processor(m_pending_adapter);
        enforce_default_processing(true);
        m_pending_adapter.reset();
    }
}
}
