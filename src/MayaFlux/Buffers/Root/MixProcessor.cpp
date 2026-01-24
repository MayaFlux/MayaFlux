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

void MixProcessor::processing_function(std::shared_ptr<Buffer> buffer)
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

            data[i] /= m_sources.size();
        }

        cleanup();
    }
}

void MixProcessor::cleanup()
{
    m_sources.erase(
        std::remove_if(m_sources.begin(), m_sources.end(),
            [](const MixSource& s) { return s.once; }),
        m_sources.end());
}

void MixProcessor::validate_sources()
{
    auto it = std::remove_if(m_sources.begin(), m_sources.end(),
        [](MixSource& s) { return !s.refresh_data(); });

    m_sources.erase(it, m_sources.end());
}

bool MixProcessor::remove_source(std::shared_ptr<AudioBuffer> buffer)
{
    if (!buffer) {
        return false;
    }

    auto it = std::remove_if(m_sources.begin(), m_sources.end(),
        [&buffer](const MixSource& s) {
            return s.matches_buffer(buffer);
        });

    if (it != m_sources.end()) {
        m_sources.erase(it, m_sources.end());
        return true;
    }

    return false;
}

}
