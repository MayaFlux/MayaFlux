#pragma once

#include "MayaFlux/Kinesis/Temporal/TimeSampling.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class FrameSource
 * @brief An ordered sequence of immutable frames, each with a time.
 *
 * A frame is whatever one time shot of a medium is: a mesh, a volume, a set of
 * points, a transform set. Frames are independent of each other, so a source can
 * be read at any index in any order and a reverse pass costs the same as a forward
 * one. A frame is shared and never changed after it is published, so a player, a
 * query and a bounds read can hold the same one with no copy and no lock, and
 * letting go of a source never frees a frame someone still holds.
 *
 * A source with one frame is a static thing. The time of each frame comes from
 * time_sampling(), a plain number in seconds resolved by whoever built the source.
 *
 * read() may be called from any thread. An implementation that decodes lazily
 * must make that safe.
 */
template <typename Frame>
class FrameSource {
public:
    virtual ~FrameSource() = default;

    /** @brief Number of frames. */
    [[nodiscard]] virtual size_t count() const = 0;

    /** @brief The time of every frame. */
    [[nodiscard]] virtual const Kinesis::TimeSampling& time_sampling() const = 0;

    /**
     * @brief The frame at an index.
     * @return The frame, or nullptr for an index past the end or a frame that
     * could not be produced.
     */
    [[nodiscard]] virtual std::shared_ptr<const Frame> read(size_t index) const = 0;

    /** @brief Place a time among the frames. */
    [[nodiscard]] std::optional<Kinesis::TimeLocation> locate(double t) const
    {
        return time_sampling().locate(t, count());
    }
};

/** @brief How a key is passed to a field: by value for a number, by reference otherwise. */
template <typename Key>
using FieldKey = std::conditional_t<std::is_arithmetic_v<Key>, Key, const Key&>;

/**
 * @class FrameField
 * @brief Frames that exist at every value of a key and are answered on demand.
 *
 * The key is whatever picks a frame: a time, a position, a feature vector, a vector of weights.
 * A frame is shared and never changed once handed out, and a field may hand out the same frame
 * for the same key. Nothing here says what the key means, so the one who reads the field
 * (a lane's selector) decides.
 *
 * sample() may be called from any thread.
 */
template <typename Frame, typename Key>
class FrameField {
public:
    virtual ~FrameField() = default;

    /**
     * @brief The frame at a key.
     * @return The frame, or nullptr for a key it cannot answer.
     */
    [[nodiscard]] virtual std::shared_ptr<const Frame> sample(FieldKey<Key> key) const = 0;
};

/**
 * @class SampledFrames
 * @brief A field of frames keyed by time: they exist at every time of a range.
 *
 * Where a FrameSource holds a finite list and a time falls among its frames, a sampled source
 * has no list: the frame at a time is computed when asked, so keyed motion, a function of time
 * or a procedural form can be played with the same courses.
 */
template <typename Frame>
class SampledFrames : public FrameField<Frame, double> {
public:
    /** @brief The first and last time, in seconds, the source answers for. The last is not before the first. */
    [[nodiscard]] virtual std::pair<double, double> extent() const = 0;
};

/**
 * @class FrameRing
 * @brief The newest frames of a stream still being written, each with the time it was written.
 *
 * A writer appends frames in time order and the oldest are dropped past the capacity, so a
 * reader can ask for a frame some seconds behind the newest: delay, echo and feedback with any
 * frame type. Frames are shared and never changed, so a reader holds one with no copy and no
 * lock. The times are the writer's, in seconds, and their rate is not fixed.
 *
 * One thread appends at a time. Any thread reads. A read that races an append sees either
 * the frames before it or after it, never a torn frame.
 *
 * As a SampledFrames it answers for the times it holds, extent() being the oldest and the
 * newest. A time before the oldest, or any time while it is empty, has no frame.
 */
template <typename Frame>
class FrameRing final : public SampledFrames<Frame> {
public:
    /** @return nullptr for a capacity of zero. */
    [[nodiscard]] static std::shared_ptr<FrameRing> create(size_t capacity)
    {
        return capacity == 0 ? nullptr : std::shared_ptr<FrameRing>(new FrameRing(capacity));
    }

    /**
     * @brief Add a frame as the newest.
     * @return false, adding nothing, for a null frame, a time that is not finite or one before the
     * newest frame's.
     */
    bool append(std::shared_ptr<const Frame> frame, double time)
    {
        if (!frame || !std::isfinite(time) || time < m_last_time) {
            return false;
        }
        const uint64_t index = m_head.load(std::memory_order_relaxed);
        m_slots[index % m_slots.size()].publish(
            std::make_shared<const Entry>(Entry { .index = index, .time = time, .frame = std::move(frame) }));
        m_last_time = time;
        m_head.store(index + 1, std::memory_order_release);
        return true;
    }

    /** @brief Frames appended since it was made. The newest has index head() - 1. */
    [[nodiscard]] uint64_t head() const noexcept { return m_head.load(std::memory_order_acquire); }

    [[nodiscard]] size_t capacity() const noexcept { return m_slots.size(); }

    /** @brief The frame appended as number @p index, or nullptr if it was dropped or not yet written. */
    [[nodiscard]] std::shared_ptr<const Frame> read(uint64_t index) const
    {
        const auto entry = entry_at(index);
        return entry ? entry->frame : nullptr;
    }

    /** @brief The index of the newest frame written at or before a time, or nullopt when there is none held. */
    [[nodiscard]] std::optional<uint64_t> locate(double time) const
    {
        for (int attempt = 0; attempt < 3; ++attempt) {
            const uint64_t head = m_head.load(std::memory_order_acquire);
            if (head == 0) {
                return std::nullopt;
            }
            uint64_t low = head > m_slots.size() ? head - m_slots.size() : 0;
            const auto first = entry_at(low);
            if (!first) {
                continue;
            }
            if (first->time > time) {
                return std::nullopt;
            }

            uint64_t high = head;
            bool lost = false;
            while (high - low > 1) {
                const uint64_t middle = low + (high - low) / 2;
                const auto entry = entry_at(middle);
                if (!entry) {
                    lost = true;
                    break;
                }
                (entry->time <= time ? low : high) = middle;
            }
            if (!lost) {
                return low;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::pair<double, double> extent() const override
    {
        const uint64_t head = m_head.load(std::memory_order_acquire);
        if (head == 0) {
            return { 0.0, 0.0 };
        }
        const auto oldest = entry_at(head > m_slots.size() ? head - m_slots.size() : 0);
        const auto newest = entry_at(head - 1);
        if (!oldest || !newest) {
            return { 0.0, 0.0 };
        }
        return { oldest->time, newest->time };
    }

    [[nodiscard]] std::shared_ptr<const Frame> sample(double time) const override
    {
        const auto index = locate(time);
        return index ? read(*index) : nullptr;
    }

private:
    struct Entry {
        uint64_t index {};
        double time {};
        std::shared_ptr<const Frame> frame;
    };

    explicit FrameRing(size_t capacity)
        : m_slots(capacity)
    {
    }

    [[nodiscard]] std::shared_ptr<const Entry> entry_at(uint64_t index) const
    {
        const auto pinned = m_slots[index % m_slots.size()].read();
        if (!pinned || !*pinned || (*pinned)->index != index) {
            return nullptr;
        }
        return *pinned;
    }

    std::vector<MayaFlux::Memory::SnapshotCell<std::shared_ptr<const Entry>>> m_slots;
    std::atomic<uint64_t> m_head { 0 };
    double m_last_time { -std::numeric_limits<double>::infinity() };
};

/**
 * @class ResidentFrames
 * @brief A FrameSource that holds every frame in memory.
 */
template <typename Frame>
class ResidentFrames final : public FrameSource<Frame> {
public:
    /**
     * @brief Hold frames under a sampling.
     * @return nullptr for no frames, a null frame, an acyclic sampling whose count
     * differs from the frames, or a sampling that cannot time the last frame.
     */
    [[nodiscard]] static std::shared_ptr<const ResidentFrames> create(
        Kinesis::TimeSampling times, std::vector<std::shared_ptr<const Frame>> frames)
    {
        if (frames.empty() || !times.time(frames.size() - 1)) {
            return nullptr;
        }
        if (const auto stored = times.stored_count(); stored && *stored != frames.size()) {
            return nullptr;
        }
        if (std::ranges::any_of(frames, [](const auto& frame) { return !frame; })) {
            return nullptr;
        }
        return std::shared_ptr<const ResidentFrames>(new ResidentFrames(std::move(times), std::move(frames)));
    }

    /** @brief Hold frames by value, each moved into its own shared frame. */
    [[nodiscard]] static std::shared_ptr<const ResidentFrames> create(
        Kinesis::TimeSampling times, std::vector<Frame> frames)
    {
        std::vector<std::shared_ptr<const Frame>> shared;
        shared.reserve(frames.size());
        for (auto& frame : frames) {
            shared.push_back(std::make_shared<const Frame>(std::move(frame)));
        }
        return create(std::move(times), std::move(shared));
    }

    [[nodiscard]] size_t count() const override { return m_frames.size(); }

    [[nodiscard]] const Kinesis::TimeSampling& time_sampling() const override { return m_times; }

    [[nodiscard]] std::shared_ptr<const Frame> read(size_t index) const override
    {
        return index < m_frames.size() ? m_frames[index] : nullptr;
    }

private:
    ResidentFrames(Kinesis::TimeSampling times, std::vector<std::shared_ptr<const Frame>> frames)
        : m_times(std::move(times))
        , m_frames(std::move(frames))
    {
    }

    Kinesis::TimeSampling m_times;
    std::vector<std::shared_ptr<const Frame>> m_frames;
};

}
