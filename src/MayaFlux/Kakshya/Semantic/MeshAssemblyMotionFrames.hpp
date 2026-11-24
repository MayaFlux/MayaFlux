#pragma once

#include "MeshAssemblyContainer.hpp"

#include "MayaFlux/Kakshya/FrameSource.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct MeshAssemblyLocals
 * @brief The local placement of every row of an assembly at one time of a motion, one matrix per row.
 */
struct MeshAssemblyLocals {
    std::vector<glm::mat4> matrices;

    /**
     * @brief The same rows placed in the world: each local composed through its parents.
     *
     * Filled by the sources in this header, so a part that shows its rows without a hierarchy
     * (a flat part) reads this and one that keeps the hierarchy reads matrices. Empty for a frame
     * made by hand, until resolve_world fills it.
     */
    std::vector<glm::mat4> world;
};

/**
 * @brief A frame of local placements with its world placements resolved through the assembly's parents.
 * @param locals One matrix per row of the assembly.
 * @return The frame, with world empty when locals has the wrong size.
 */
[[nodiscard]] inline MeshAssemblyLocals resolve_world(
    const MeshAssemblyContainer& assembly, std::vector<glm::mat4> locals)
{
    MeshAssemblyLocals frame;
    frame.world = assembly.world_transforms(locals);
    frame.matrices = std::move(locals);
    return frame;
}

/**
 * @class MeshAssemblyMotionFrames
 * @brief A motion of a MeshAssemblyContainer as a SampledFrames of MeshAssemblyLocals.
 *
 * Every time of the motion's range answers with the placements the motion's keys give there, cubic
 * splines and any key spacing included, so a Reflow plays a motion without baking it to frames.
 * The range runs from the earliest key of any channel to the latest.
 *
 * Holds the assembly, so the motion stays readable for as long as the source lives. The assembly
 * must not be changed while the source is read.
 */
class MeshAssemblyMotionFrames final : public SampledFrames<MeshAssemblyLocals> {
public:
    /**
     * @brief Make the source for one motion.
     * @param assembly The assembly. Null gives nullptr.
     * @param motion Motion index.
     * @param policy What a channel answers outside its own keys.
     * @return nullptr for a null assembly, a motion out of range or a motion with no keyed component.
     */
    [[nodiscard]] static MAYAFLUX_API std::shared_ptr<const MeshAssemblyMotionFrames> create(
        const std::shared_ptr<const MeshAssemblyContainer>& assembly, size_t motion,
        Kinesis::OutOfRange policy = Kinesis::OutOfRange::Hold);

    [[nodiscard]] std::pair<double, double> extent() const override { return { m_first, m_last }; }

    [[nodiscard]] MAYAFLUX_API std::shared_ptr<const MeshAssemblyLocals> sample(double time) const override;

private:
    MeshAssemblyMotionFrames(const std::shared_ptr<const MeshAssemblyContainer>& assembly, size_t motion,
        Kinesis::OutOfRange policy, double first, double last)
        : m_assembly(assembly)
        , m_motion(motion)
        , m_policy(policy)
        , m_first(first)
        , m_last(last)
    {
    }

    std::shared_ptr<const MeshAssemblyContainer> m_assembly;
    size_t m_motion;
    Kinesis::OutOfRange m_policy;
    double m_first;
    double m_last;
};

/**
 * @class MeshAssemblyRestFrames
 * @brief The rest placement of an assembly as a SampledFrames of MeshAssemblyLocals: one frame,
 *        the same at every time.
 *
 * For an assembly with no motion, so a reflow has frames to place its parts with, and so the frame
 * can be edited every cycle (Kriya::placement_edit) by a field, a cell or a Nexus influence. The range
 * is 0 to 1 second, and every time answers with the one shared frame. Holds the assembly.
 */
class MeshAssemblyRestFrames final : public SampledFrames<MeshAssemblyLocals> {
public:
    /** @return nullptr for a null assembly or one with no rows. */
    [[nodiscard]] static MAYAFLUX_API std::shared_ptr<const MeshAssemblyRestFrames> create(
        const std::shared_ptr<const MeshAssemblyContainer>& assembly);

    [[nodiscard]] std::pair<double, double> extent() const override { return { 0.0, 1.0 }; }

    [[nodiscard]] std::shared_ptr<const MeshAssemblyLocals> sample(double) const override { return m_frame; }

private:
    explicit MeshAssemblyRestFrames(std::shared_ptr<const MeshAssemblyLocals> frame)
        : m_frame(std::move(frame))
    {
    }

    std::shared_ptr<const MeshAssemblyLocals> m_frame;
};

} // namespace MayaFlux::Kakshya
