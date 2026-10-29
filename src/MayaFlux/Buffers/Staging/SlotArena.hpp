#pragma once

#include "MayaFlux/Buffers/VKBuffer.hpp"

namespace MayaFlux::Buffers {

/**
 * @class SlotArena
 * @brief One host-visible buffer split into equal slots, with the slot chosen where the descriptor is pushed
 *
 * For data the host rewrites while the GPU may still be reading an earlier
 * version of it. Instead of writing into the buffer a pending frame reads,
 * the owner writes a slot nothing is reading and pushes that slot's range.
 * Nothing waits: a slot is free when the epoch of its last read has
 * completed, and when none is free the arena grows.
 *
 * Epochs are numbers the owner defines, compared only against each other.
 * The owner stamps a slot with the epoch that will read it (mark_read) and
 * passes the highest finished epoch to acquire(). The arena knows nothing
 * about windows, frames or queues.
 *
 * Slots are aligned to 256 bytes, the largest uniform buffer offset alignment
 * Vulkan permits, so a slot's range is valid for uniform and storage bindings
 * on every device.
 */
class MAYAFLUX_API SlotArena {
public:
    SlotArena() = default;
    ~SlotArena() = default;

    SlotArena(const SlotArena&) = delete;
    SlotArena& operator=(const SlotArena&) = delete;
    SlotArena(SlotArena&&) = delete;
    SlotArena& operator=(SlotArena&&) = delete;

    /**
     * @brief Allocate the arena, releasing any previous buffer
     * @param slot_bytes Bytes each slot holds
     * @param usage Buffer usage, which must be host visible
     * @param slots Initial slot count
     * @return True if the buffer is allocated and mapped
     */
    bool initialize(size_t slot_bytes, VKBuffer::Usage usage, size_t slots = 2);

    [[nodiscard]] bool is_initialized() const noexcept { return m_buffer != nullptr; }

    /**
     * @brief A slot whose last read has completed, growing the arena if none is
     * @param completed Highest epoch known to have finished
     */
    [[nodiscard]] size_t acquire(uint64_t completed);

    /**
     * @brief Copy bytes into a slot and flush them
     * @param slot Slot from acquire()
     * @param data Source bytes
     * @param size Byte count, at most the slot size
     */
    void write(size_t slot, const void* data, size_t size);

    /**
     * @brief Record the epoch that will read a slot
     */
    void mark_read(size_t slot, uint64_t epoch);

    /**
     * @brief Buffer, offset and range of a slot, ready for a descriptor write
     */
    [[nodiscard]] vk::DescriptorBufferInfo info(size_t slot) const;

    [[nodiscard]] size_t slot_count() const noexcept { return m_read_epoch.size(); }

    [[nodiscard]] size_t slot_bytes() const noexcept { return m_slot_bytes; }

    /**
     * @brief Hand the buffer to the caller, leaving the arena empty
     *
     * For an owner that keeps the buffer alive until its readers finish.
     */
    [[nodiscard]] std::shared_ptr<VKBuffer> detach();

    /**
     * @brief Release the buffer once the work submitted so far completes
     */
    void release();

private:
    void grow();

    std::shared_ptr<VKBuffer> m_buffer;
    std::vector<uint64_t> m_read_epoch;
    size_t m_slot_bytes {};
    size_t m_stride {};
    VKBuffer::Usage m_usage {};
};

} // namespace MayaFlux::Buffers
