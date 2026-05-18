#pragma once

#include "DynamicSoundStream.hpp"
#include "MayaFlux/Core/GlobalStreamInfo.hpp"

namespace MayaFlux::Kakshya {

class AudioOutputAccessProcessor;

/**
 * @class AudioOutputContainer
 * @brief DynamicSoundStream subclass wrapping the live engine audio output.
 *
 * Exposes each completed output cycle as addressable N-dimensional data,
 * symmetric with WindowContainer's treatment of window surfaces.
 *
 * Data model:
 *   m_processed_data — current cycle snapshot, written by AudioOutputAccessProcessor
 *                       each process() call. Hot consumable for real-time nodes,
 *                       visualizers, and metering.
 *   m_data           — unbounded accumulating history of all output frames written
 *                       so far, growing via write_frames() each cycle. The write
 *                       head is always get_num_frames(). Readers attach independent
 *                       CursorAccessProcessor instances against this container and
 *                       chase the write head at their own rate.
 *
 * Multi-reader model:
 *   Inherited from DynamicSoundStream. Each consumer (SoundFileWriter, network
 *   broadcast, metering) calls allocate_dynamic_slot() and attaches a
 *   CursorAccessProcessor with set_loop_region(reader_pos, get_num_frames()).
 *   The container never pushes to readers; readers pull on their own schedule.
 *
 * Processing model:
 *   The default processor (AudioOutputAccessProcessor) pulls the engine snapshot
 *   once per process() call, writes the deinterleaved result into m_processed_data,
 *   then appends the same data to m_data via write_frames(). The container never
 *   calls process() itself; callers drive it via process_default() hooked into
 *   an AudioBackendService observer or BroadcastSource.
 *
 * Position semantics:
 *   m_read_position is unused as a navigation cursor here. The write head
 *   (get_num_frames()) is the only meaningful position. StreamContainer
 *   position purv virtuals are inherited from SoundStreamContainer and operate
 *   correctly against m_data for any reader that chooses to use them.
 */
class MAYAFLUX_API AudioOutputContainer : public DynamicSoundStream {
public:
    /**
     * @brief Construct from stream configuration.
     * @param stream_info  Provides channel count, buffer size, and sample rate.
     */
    explicit AudioOutputContainer(Core::GlobalStreamInfo stream_info);

    ~AudioOutputContainer() override = default;

    AudioOutputContainer(const AudioOutputContainer&) = delete;
    AudioOutputContainer& operator=(const AudioOutputContainer&) = delete;
    AudioOutputContainer(AudioOutputContainer&&) = delete;
    AudioOutputContainer& operator=(AudioOutputContainer&&) = delete;

    [[nodiscard]] uint32_t get_buffer_size() const { return m_buffer_size; }

    /**
     * @brief Instantiate and attach an AudioOutputAccessProcessor as the default processor.
     *
     * Overrides DynamicSoundStream::create_default_processor which would
     * attach a ContiguousAccessProcessor unsuitable for a live output source.
     */
    void create_default_processor() override;

    /**
     * @brief Drive one cycle: pull snapshot, update m_processed_data, append to m_data.
     *
     * Overrides SoundStreamContainer::process_default to skip the
     * is_ready_for_processing guard — the container is always ready once
     * the backend has produced at least one cycle.
     */
    void process_default() override;

private:
    Core::GlobalStreamInfo m_stream_info;
    uint32_t m_buffer_size;

    friend class AudioOutputAccessProcessor;
};

} // namespace MayaFlux::Kakshya
