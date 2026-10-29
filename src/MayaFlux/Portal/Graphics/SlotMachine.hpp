#pragma once

#include "GraphicsUtils.hpp"

#include "vulkan/vulkan.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @class SlotLease
 * @brief A claim on one slot of a SlotArena for as long as the claim exists
 *
 * Copies and moves share the claim; the slot is in use until every copy is
 * gone. A command buffer that pushed the slot's range holds one until its
 * submission completes, so the slot cannot be reused while the GPU may read it.
 */
class MAYAFLUX_API SlotLease {
public:
    SlotLease() = default;

    explicit SlotLease(std::shared_ptr<std::atomic<uint32_t>> uses)
        : m_uses(std::move(uses))
    {
        if (m_uses) {
            m_uses->fetch_add(1, std::memory_order_relaxed);
        }
    }

    SlotLease(const SlotLease& other)
        : m_uses(other.m_uses)
    {
        if (m_uses) {
            m_uses->fetch_add(1, std::memory_order_relaxed);
        }
    }

    SlotLease(SlotLease&& other) noexcept = default;

    SlotLease& operator=(const SlotLease& other)
    {
        if (this != &other) {
            release();
            m_uses = other.m_uses;
            if (m_uses) {
                m_uses->fetch_add(1, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    SlotLease& operator=(SlotLease&& other) noexcept
    {
        if (this != &other) {
            release();
            m_uses = std::move(other.m_uses);
        }
        return *this;
    }

    ~SlotLease() { release(); }

    [[nodiscard]] explicit operator bool() const noexcept { return m_uses != nullptr; }

private:
    void release() noexcept
    {
        if (m_uses) {
            m_uses->fetch_sub(1, std::memory_order_release);
            m_uses.reset();
        }
    }

    std::shared_ptr<std::atomic<uint32_t>> m_uses;
};

/**
 * @class SlotArena
 * @brief One host-visible buffer split into equal slots, with the slot chosen where the descriptor is pushed
 *
 * For data the host rewrites while the GPU may still be reading an earlier
 * version of it. The owner writes a slot nothing holds a lease on and pushes
 * that slot's range. Whoever records the push takes a lease on the slot and
 * the command buffer keeps it until its submission completes. Nothing waits:
 * acquire() returns a slot with no lease, and grows the arena when every
 * slot is held.
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
    bool initialize(size_t slot_bytes, BufferUsageHint usage, size_t slots = 2);

    [[nodiscard]] bool is_initialized() const noexcept { return m_buffer != nullptr; }

    /**
     * @brief A slot nothing holds a lease on, growing the arena if none is free
     */
    [[nodiscard]] size_t acquire();

    /**
     * @brief Copy bytes into a slot and flush them
     * @param slot Slot from acquire()
     * @param data Source bytes
     * @param size Byte count, at most the slot size
     */
    void write(size_t slot, const void* data, size_t size);

    /**
     * @brief Claim a slot until the returned lease and its copies are gone
     */
    [[nodiscard]] SlotLease lease(size_t slot) const;

    /**
     * @brief Buffer, offset and range of a slot, ready for a descriptor write
     */
    [[nodiscard]] vk::DescriptorBufferInfo info(size_t slot) const;

    [[nodiscard]] size_t slot_count() const noexcept { return m_uses.size(); }

    [[nodiscard]] size_t slot_bytes() const noexcept { return m_slot_bytes; }

    /**
     * @brief The backing buffer, which changes when the arena grows
     */
    [[nodiscard]] const std::shared_ptr<Buffers::VKBuffer>& buffer() const noexcept { return m_buffer; }

    /**
     * @brief Hand the buffer to the caller, leaving the arena empty
     *
     * For an owner that keeps the buffer alive until its readers finish.
     */
    [[nodiscard]] std::shared_ptr<Buffers::VKBuffer> detach();

    /**
     * @brief Release the buffer once the work submitted so far completes
     */
    void release();

private:
    bool grow();

    std::shared_ptr<Buffers::VKBuffer> m_buffer;
    std::vector<std::shared_ptr<std::atomic<uint32_t>>> m_uses;
    size_t m_slot_bytes {};
    size_t m_stride {};
    BufferUsageHint m_usage {};
};

} // namespace MayaFlux::Portal::Graphics
