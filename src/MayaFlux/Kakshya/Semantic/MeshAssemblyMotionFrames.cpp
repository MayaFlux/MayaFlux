#include "MeshAssemblyMotionFrames.hpp"

namespace MayaFlux::Kakshya {

namespace {

    template <class Keyed>
    void widen(const std::optional<Keyed>& keys, double& first, double& last, bool& any)
    {
        if (!keys) {
            return;
        }
        first = any ? std::min(first, keys->first_time()) : keys->first_time();
        last = any ? std::max(last, keys->last_time()) : keys->last_time();
        any = true;
    }

}

std::shared_ptr<const MeshAssemblyMotionFrames> MeshAssemblyMotionFrames::create(
    std::shared_ptr<const MeshAssemblyContainer> assembly, size_t motion, Kinesis::OutOfRange policy)
{
    if (!assembly || motion >= assembly->get_data().motions.size()) {
        return nullptr;
    }

    double first = 0.0;
    double last = 0.0;
    bool any = false;
    for (const auto& channel : assembly->get_data().motions[motion].channels) {
        widen(channel.translation, first, last, any);
        widen(channel.rotation, first, last, any);
        widen(channel.scale, first, last, any);
    }
    if (!any) {
        return nullptr;
    }

    return std::shared_ptr<const MeshAssemblyMotionFrames>(
        new MeshAssemblyMotionFrames(std::move(assembly), motion, policy, first, last));
}

std::shared_ptr<const MeshAssemblyLocals> MeshAssemblyMotionFrames::sample(double time) const
{
    auto matrices = m_assembly->sample(m_motion, time, m_policy);
    if (!matrices) {
        return nullptr;
    }
    return std::make_shared<const MeshAssemblyLocals>(MeshAssemblyLocals { .matrices = std::move(*matrices) });
}

} // namespace MayaFlux::Kakshya
