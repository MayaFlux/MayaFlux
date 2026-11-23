#pragma once

#include "MayaFlux/Kriya/Course.hpp"

namespace MayaFlux::Kriya {

/**
 * @enum FrameSelection
 * @brief Which frames a course's time picks from a source.
 */
enum class FrameSelection : uint8_t {
    Floor, ///< The frame at or before the time.
    Nearest, ///< The closer of the two frames around the time.
    Blend ///< Both frames around the time, weighted by how near each is.
};

/**
 * @enum Rewrite
 * @brief When a reflow hands the sink a frame it has already handed over.
 */
enum class Rewrite : uint8_t {
    OnChange, ///< Only when the frame is not the one last written.
    Always ///< Every cycle, as a sink that adds into a running simulation wants.
};

/**
 * @struct WeightedFrame
 * @brief One frame a course picked, with how much it counts.
 */
template <typename Frame>
struct WeightedFrame {
    std::shared_ptr<const Frame> frame;
    double weight {};
    size_t index {};
};

/**
 * @struct FrameContext
 * @brief What every hook on a frame in flight is given.
 *
 * The courses are the live ones, so a hook may read their cells and queue commands to
 * them, which is how what one frame shows steers the time of the next. Valid for the
 * call only.
 */
struct FrameContext {
    std::span<const std::shared_ptr<Course>> courses;
    uint64_t cycle {};
    const Course* course {}; ///< The course the frame came from, or null where several have been mixed.
};

/**
 * @class FrameOperation
 * @brief A typed hook on the frame of a reflow between its selection and its sink.
 *
 * Frames are immutable and shared, and every hook is handed the shared frame, so one that
 * wants to keep a frame (to record it, to compare with the next) holds the pointer and
 * never copies the frame. A transform returns a new shared frame and never edits the one
 * it was given. Operations run in the order they were added.
 *
 * - transform: frame to frame. Null means nothing is written this cycle.
 * - derive: reads the frame and returns nothing. Publishes what it finds, such as a
 *   bound or a peak, into cells or onto a course.
 * - condition: false means nothing is written this cycle.
 * - mix: several frames to one, when more than one course contributes. At most one per
 *   reflow, and without one the heaviest frame wins.
 */
template <typename Frame>
class FrameOperation {
public:
    enum class Kind : uint8_t {
        Transform,
        Derive,
        Condition,
        Mix
    };

    using TransformFn = std::function<std::shared_ptr<const Frame>(const FrameContext&, std::shared_ptr<const Frame>)>;
    using DeriveFn = std::function<void(const FrameContext&, const std::shared_ptr<const Frame>&)>;
    using ConditionFn = std::function<bool(const FrameContext&, const std::shared_ptr<const Frame>&)>;
    using MixFn = std::function<std::shared_ptr<const Frame>(const FrameContext&, std::span<const WeightedFrame<Frame>>)>;

    static FrameOperation transform(TransformFn fn)
    {
        FrameOperation op(Kind::Transform);
        op.m_transform = std::move(fn);
        return op;
    }

    static FrameOperation derive(DeriveFn fn)
    {
        FrameOperation op(Kind::Derive);
        op.m_derive = std::move(fn);
        return op;
    }

    static FrameOperation condition(ConditionFn fn)
    {
        FrameOperation op(Kind::Condition);
        op.m_condition = std::move(fn);
        return op;
    }

    static FrameOperation mix(MixFn fn)
    {
        FrameOperation op(Kind::Mix);
        op.m_mix = std::move(fn);
        return op;
    }

    [[nodiscard]] Kind kind() const noexcept { return m_kind; }

    /** @brief Whether the callable it was made with can be called. */
    [[nodiscard]] bool valid() const
    {
        switch (m_kind) {
        case Kind::Transform:
            return static_cast<bool>(m_transform);
        case Kind::Derive:
            return static_cast<bool>(m_derive);
        case Kind::Condition:
            return static_cast<bool>(m_condition);
        case Kind::Mix:
            return static_cast<bool>(m_mix);
        }
        return false;
    }

    [[nodiscard]] const TransformFn& transform_fn() const noexcept { return m_transform; }
    [[nodiscard]] const DeriveFn& derive_fn() const noexcept { return m_derive; }
    [[nodiscard]] const ConditionFn& condition_fn() const noexcept { return m_condition; }
    [[nodiscard]] const MixFn& mix_fn() const noexcept { return m_mix; }

private:
    explicit FrameOperation(Kind kind)
        : m_kind(kind)
    {
    }

    Kind m_kind;
    TransformFn m_transform;
    DeriveFn m_derive;
    ConditionFn m_condition;
    MixFn m_mix;
};

} // namespace MayaFlux::Kriya
