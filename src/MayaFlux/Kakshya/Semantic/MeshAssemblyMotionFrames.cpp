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
    const std::shared_ptr<const MeshAssemblyContainer>& assembly, size_t motion, Kinesis::OutOfRange policy)
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
    for (const auto& weights : assembly->get_data().motions[motion].weights) {
        for (const auto& keys : weights.targets) {
            widen(std::optional<Kinesis::TimedValues<float>> { keys }, first, last, any);
        }
    }
    if (!any) {
        return nullptr;
    }

    return std::shared_ptr<const MeshAssemblyMotionFrames>(
        new MeshAssemblyMotionFrames(assembly, motion, policy, first, last));
}

std::shared_ptr<const MeshAssemblyLocals> MeshAssemblyMotionFrames::sample(double time) const
{
    auto matrices = m_assembly->sample(m_motion, time, m_policy);
    if (!matrices) {
        return nullptr;
    }

    auto frame = resolve_world(*m_assembly, std::move(*matrices));
    const auto& data = m_assembly->get_data();
    for (const auto& weights : data.motions[m_motion].weights) {
        const auto placement = m_assembly->placement(weights.row);
        const auto* morphs = placement && placement->source
            ? data.sources[*placement->source].facets.get<MeshMorphTargets>()
            : nullptr;
        if (!morphs) {
            continue;
        }
        frame.weights.resize(m_assembly->get_total_elements());
        auto& row = frame.weights[weights.row];
        row.reserve(morphs->size());
        for (size_t t = 0; t < morphs->size(); ++t) {
            const auto value = t < weights.targets.size() ? weights.targets[t].sample(time, m_policy) : std::nullopt;
            row.push_back(value.value_or((*morphs)[t].second.front().weight));
        }
    }
    return std::make_shared<const MeshAssemblyLocals>(std::move(frame));
}

std::shared_ptr<const MeshAssemblyRestFrames> MeshAssemblyRestFrames::create(
    const std::shared_ptr<const MeshAssemblyContainer>& assembly)
{
    if (!assembly || assembly->get_total_elements() == 0) {
        return nullptr;
    }
    return std::shared_ptr<const MeshAssemblyRestFrames>(new MeshAssemblyRestFrames(
        std::make_shared<const MeshAssemblyLocals>(resolve_world(*assembly, assembly->local_transforms()))));
}

} // namespace MayaFlux::Kakshya
