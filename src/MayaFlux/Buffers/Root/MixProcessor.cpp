#include "MixProcessor.hpp"

#include "RootAudioBuffer.hpp"

namespace MayaFlux::Buffers {

MixSource::MixSource(const std::shared_ptr<AudioBuffer>& buffer, double level, bool once_flag)
    : mix_level(level)
    , once(once_flag)
    , buffer_ref(buffer)
{
    if (buffer) {
        data = buffer->get_data();
    }
}

bool MixSource::refresh_data()
{
    if (buffer_ref.expired()) {
        return false;
    }

    if (auto buffer = buffer_ref.lock()) {
        data = buffer->get_data();
        return !data.empty();
    }

    return false;
}

bool MixProcessor::register_source(std::shared_ptr<AudioBuffer> source, double mix_level, bool once)
{
    if (!source || source->get_data().empty()) {
        return false;
    }

    auto it = std::ranges::find_if(m_sources,
        [&source](const MixSource& s) {
            return s.matches_buffer(source);
        });

    if (it != m_sources.end()) {
        it->mix_level = mix_level;
        it->once = once;
        it->refresh_data();
        return true;
    }

    m_sources.emplace_back(source, mix_level, once);
    return true;
}

void MixProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_sources.empty()) {
        return;
    }

    if (auto root_buffer = std::dynamic_pointer_cast<RootAudioBuffer>(buffer)) {
        validate_sources();

        auto& data = root_buffer->get_data();

        for (uint32_t i = 0; i < data.size(); i++) {
            for (const auto& source : m_sources) {
                if (source.has_sample_at(i)) {
                    data[i] += source.get_mixed_sample(i);
                }
            }

            data[i] /= (double)m_sources.size();
        }

        cleanup();
    }
}

void MixProcessor::cleanup()
{
    std::erase_if(m_sources, [](const MixSource& s) { return s.once; });
}

void MixProcessor::validate_sources()
{
    std::erase_if(m_sources, [](MixSource& s) { return !s.refresh_data(); });
}

bool MixProcessor::remove_source(const std::shared_ptr<AudioBuffer>& buffer)
{
    if (!buffer) {
        return false;
    }

    auto erased = std::erase_if(m_sources, [&buffer](const MixSource& s) {
        return s.matches_buffer(buffer);
    });

    return erased > 0;
}

bool MixProcessor::update_source_mix(const std::shared_ptr<AudioBuffer>& buffer, double new_mix_level)
{
    for (auto& source : m_sources) {
        if (source.matches_buffer(buffer)) {
            source.mix_level = new_mix_level;
            return true;
        }
    }
    return false;
}

}
