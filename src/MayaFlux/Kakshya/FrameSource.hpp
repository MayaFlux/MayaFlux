#pragma once

#include "MayaFlux/Kinesis/Temporal/TimeSampling.hpp"

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

/**
 * @class SampledFrames
 * @brief A source of frames that exist at every time of a range and are answered on demand.
 *
 * Where a FrameSource holds a finite list and a time falls among its frames, a sampled source
 * has no list: the frame at a time is computed when asked, so keyed motion, a function of time
 * or a procedural form can be played with the same courses. A frame is shared and never changed
 * once handed out, and a source may hand out the same frame for the same time.
 *
 * sample() may be called from any thread.
 */
template <typename Frame>
class SampledFrames {
public:
    virtual ~SampledFrames() = default;

    /** @brief The first and last time, in seconds, the source answers for. The last is not before the first. */
    [[nodiscard]] virtual std::pair<double, double> extent() const = 0;

    /**
     * @brief The frame at a time.
     * @return The frame, or nullptr for a time it cannot answer.
     */
    [[nodiscard]] virtual std::shared_ptr<const Frame> sample(double time) const = 0;
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
