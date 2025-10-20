#pragma once
#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Buffers {

class AudioBuffer;

/**
 * @struct MixSource
 * @brief Represents a source audio buffer with its data and mixing properties.
 *
 * This structure holds a reference to an audio buffer and its data, along with
 * the mix level and a flag indicating if it should be mixed only once.
 */
struct MAYAFLUX_API MixSource {
    std::span<double> data;
    double mix_level = 1.0;
    bool once = false;

    MixSource(std::shared_ptr<AudioBuffer> buffer, double level = 1.0, bool once_flag = false);

    bool is_valid() const
    {
        return !buffer_ref.expired() && !data.empty();
    }

    bool refresh_data();

    bool matches_buffer(const std::shared_ptr<AudioBuffer>& buffer) const
    {
        return !buffer_ref.expired() && buffer_ref.lock() == buffer;
    }

    inline bool has_sample_at(size_t index) const
    {
        return index < data.size();
    }

    inline double get_mixed_sample(size_t index) const
    {
        return data[index] * mix_level;
    }

private:
    std::weak_ptr<AudioBuffer> buffer_ref;
};

/**
 * @class MixProcessor
 * @brief Processes multiple audio buffers and mixes their data into a single output buffer.
 *
 * This processor allows multiple audio sources to be registered, each with its own mix level.
 * It can handle both continuous mixing and one-time mixes based on the `once` flag.
 *
 * This is the primary mechanism for single audio buffer to be mixed into multiple channels.
 * Compared to Nodes, buffers are inherently single channel due to their transient nature and
 * the architecture of MayaFlux that adds processors to buffers instead of processing buffers themselves
 * Hence, process once and supply to multiple channels is the most efficient method to send concurrent data
 * to multiple channels.
 */
class MAYAFLUX_API MixProcessor : public BufferProcessor {
public:
    /**
     * @brief register an AudioBuffer source to be mixed into the output of specified channel
     * @param source AudioBuffer to register
     * @param mix_level Level of mixing for this source (default: 1.0)
     * @param once If true, the source will be mixed only once and then removed (default: false)
     */
    bool register_source(std::shared_ptr<AudioBuffer> source, double mix_level = 1.0, bool once = false);

    /** @brief the mechanism to mix output from one buffer to another channel */
    void processing_function(std::shared_ptr<Buffer> buffer) override;

    /**
     * @brief Removes a source buffer from the mix
     * @param buffer Buffer to remove
     * @return true if the source was successfully removed, false if it was not found
     */
    bool remove_source(std::shared_ptr<AudioBuffer> buffer);

private:
    void cleanup();

    void validate_sources();

    std::vector<MixSource> m_sources;
};

}
