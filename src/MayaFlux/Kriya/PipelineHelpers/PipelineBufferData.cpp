#include "PipelineBufferData.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

namespace MayaFlux::Kriya::detail {

Kakshya::DataVariant extract_buffer_data(const std::shared_ptr<Buffers::AudioBuffer>& buffer, bool should_process)
{
    auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
    if (audio_buffer) {
        if (should_process) {
            audio_buffer->process_default();
        }
        const auto& data_span = audio_buffer->get_data();
        std::vector<double> data_vector(data_span.begin(), data_span.end());
        return data_vector;
    }

    return std::vector<double> {};
}

void write_to_buffer(const std::shared_ptr<Buffers::AudioBuffer>& buffer, const Kakshya::DataVariant& data)
{
    auto audio_buffer = std::dynamic_pointer_cast<Buffers::AudioBuffer>(buffer);
    if (audio_buffer) {
        try {
            auto audio_data = std::get<std::vector<double>>(data);
            auto& buffer_data = audio_buffer->get_data();

            if (buffer_data.size() != audio_data.size()) {
                buffer_data.resize(audio_data.size());
            }

            std::ranges::copy(audio_data, buffer_data.begin());

        } catch (const std::bad_variant_access& e) {
            error_rethrow(Journal::Component::Kriya,
                Journal::Context::CoroutineScheduling,
                std::source_location::current(),
                "Data type mismatch when writing to audio buffer: {}",
                e.what());
        }
    }
}

void write_to_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container, const Kakshya::DataVariant& data, uint32_t channel)
{
    try {
        const auto& audio_data = std::get<std::vector<double>>(data);
        std::span<const double> data_span(audio_data.data(), audio_data.size());

        container->append_frames(data_span, channel);

    } catch (const std::bad_variant_access& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Data type mismatch when writing to container: {}",
            e.what());
    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Error writing to container: {}",
            e.what());
    }
}

Kakshya::DataVariant read_from_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container,
    uint64_t,
    uint32_t length)
{
    try {
        uint32_t read_length = length;
        if (read_length == 0) {
            read_length = static_cast<uint32_t>(container->get_total_elements() / container->get_num_channels());
        }

        std::vector<double> output_data(static_cast<size_t>(read_length * container->get_num_channels()));
        std::span<double> output_span(output_data.data(), output_data.size());

        uint64_t frames_read = container->read_frames(output_span, read_length);

        if (frames_read < output_data.size()) {
            output_data.resize(frames_read);
        }

        return output_data;

    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Kriya,
            Journal::Context::CoroutineScheduling,
            std::source_location::current(),
            "Error reading from container: {}",
            e.what());

        return std::vector<double> {};
    }
}

}
