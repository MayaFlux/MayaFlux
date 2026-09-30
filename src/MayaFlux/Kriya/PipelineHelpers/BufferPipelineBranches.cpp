#include "MayaFlux/Kriya/BufferPipeline.hpp"

#include "MayaFlux/Kriya/CycleCoordinator.hpp"

namespace MayaFlux::Kriya {

BufferPipeline& BufferPipeline::branch_if(
    std::function<bool(uint32_t)> condition,
    const std::function<void(BufferPipeline&)>& branch_builder,
    bool synchronous,
    uint64_t samples_per_operation)
{
    auto branch_pipeline = std::make_shared<BufferPipeline>();
    if (m_scheduler) {
        branch_pipeline->m_scheduler = m_scheduler;
    }
    branch_builder(*branch_pipeline);

    m_branches.push_back({ .condition = std::move(condition),
        .pipeline = std::move(branch_pipeline),
        .synchronous = synchronous,
        .samples_per_operation = samples_per_operation });

    return *this;
}

std::shared_ptr<Vruta::SoundRoutine> BufferPipeline::dispatch_branch_async(BranchInfo& branch, uint64_t)
{
    if (!m_scheduler)
        return nullptr;

    if (!m_coordinator) {
        m_coordinator = std::make_unique<CycleCoordinator>(*m_scheduler);
    }

    branch.pipeline->m_active_self = branch.pipeline;

    auto branch_routine = branch.pipeline->execute_internal(1, branch.samples_per_operation);

    auto task = std::make_shared<Vruta::SoundRoutine>(std::move(branch_routine));
    m_scheduler->add_task(task);

    m_branch_tasks.push_back(task);

    return task;
}

void BufferPipeline::cleanup_completed_branches()
{
    for (auto& branch : m_branches) {
        if (branch.pipeline) {
            branch.pipeline->m_active_self.reset();
        }
    }

    std::erase_if(m_branch_tasks, [](const auto& task) {
        return !task || !task->is_active();
    });
}

}
