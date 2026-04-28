#include "Bridge.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Forma/FormaBindingsProcessor.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryWriteProcessor.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"

#include "MayaFlux/Kriya/Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Kriya/Awaiters/GetPromise.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

#include "MayaFlux/Nodes/Conduit/Constant.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Forma {

Bridge::Bridge(
    Vruta::TaskScheduler& scheduler,
    Buffers::BufferManager& buffer_manager)
    : m_scheduler(scheduler)
    , m_buffer_manager(buffer_manager)
{
}

Bridge::~Bridge()
{
    for (auto& [id, rec] : m_records) {
        cancel_inbound(rec);
        cancel_outbound(rec);
    }
}

// =============================================================================
// Inbound
// =============================================================================

void Bridge::bind(
    uint32_t id,
    std::shared_ptr<Nodes::Node> node,
    std::function<float(double)> project)
{
    auto reader = project
        ? std::function<float()>([n = std::move(node), p = std::move(project)] {
              return p(n->get_last_output());
          })
        : std::function<float()>([n = std::move(node)] {
              return static_cast<float>(n->get_last_output());
          });

    spawn_inbound(id, std::move(reader));
}

void Bridge::bind(uint32_t id, std::function<float()> source)
{
    spawn_inbound(id, std::move(source));
}

void Bridge::spawn_inbound(uint32_t id, std::function<float()> source)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::bind: unknown element id {}", id);
        return;
    }

    cancel_inbound(it->second);

    auto name = make_task_name(id, "inbound");
    it->second.inbound_task = name;

    auto& rec = it->second;
    auto writer = rec.writer;

    auto routine = [](Vruta::TaskScheduler&,
                       std::function<float()> src,
                       std::function<void(float)> write) -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate) {
            write(src());
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(source), std::move(writer))),
        name, false);
}

// =============================================================================
// Outbound
// =============================================================================

void Bridge::write(
    uint32_t id,
    std::shared_ptr<Buffers::ShaderProcessor> target,
    uint32_t offset,
    size_t size)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", id);
        return;
    }

    auto& rec = it->second;
    if (!rec.bindings) {
        rec.bindings = std::make_shared<Buffers::FormaBindingsProcessor>(
            target->get_shader_path());
        m_buffer_manager.add_processor(
            rec.bindings, rec.buffer,
            Buffers::ProcessingToken::GRAPHICS_BACKEND);
    }

    rec.bindings->bind_push_constant(
        std::to_string(id) + "_pc_" + std::to_string(offset),
        rec.reader,
        std::move(target),
        offset, size);
}

void Bridge::write(
    uint32_t id,
    const std::string& descriptor_name,
    uint32_t binding_index,
    uint32_t set,
    Portal::Graphics::DescriptorRole role)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", id);
        return;
    }

    auto& rec = it->second;
    if (!rec.bindings) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write(descriptor): attach a ShaderProcessor binding first "
            "so the FormaBindingsProcessor has a shader path");
        return;
    }

    rec.bindings->bind_descriptor(
        descriptor_name + "_" + std::to_string(id),
        rec.reader,
        descriptor_name,
        binding_index, set, role);
}

void Bridge::write(uint32_t id, std::shared_ptr<Buffers::AudioWriteProcessor> target)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", id);
        return;
    }

    auto name = make_task_name(id, "audio");
    it->second.outbound_tasks.push_back(name);

    auto reader = it->second.reader;

    auto routine = [](Vruta::TaskScheduler&,
                       std::function<float()> r,
                       std::shared_ptr<Buffers::AudioWriteProcessor> proc)
        -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate) {
            proc->set_data(std::vector<double> { static_cast<double>(r()) });
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(reader), std::move(target))),
        name, false);
}

void Bridge::write(uint32_t id, std::shared_ptr<Buffers::GeometryWriteProcessor> target)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", id);
        return;
    }

    auto name = make_task_name(id, "geometry");
    it->second.outbound_tasks.push_back(name);

    auto reader = it->second.reader;

    auto routine = [](Vruta::TaskScheduler&,
                       std::function<float()> r,
                       std::shared_ptr<Buffers::GeometryWriteProcessor> proc)
        -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate) {
            proc->set_data(Kakshya::DataVariant { std::vector<float> { r() } });
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(reader), std::move(target))),
        name, false);
}

void Bridge::write(uint32_t id, std::shared_ptr<Nodes::Constant> node)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", id);
        return;
    }

    auto name = make_task_name(id, "constant");
    it->second.outbound_tasks.push_back(name);

    auto reader = it->second.reader;

    auto routine = [](Vruta::TaskScheduler&,
                       std::function<float()> r,
                       std::shared_ptr<Nodes::Constant> n)
        -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate) {
            n->set_constant(static_cast<double>(r()));
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(reader), std::move(node))),
        name, false);
}

// =============================================================================
// Lifecycle
// =============================================================================

void Bridge::unbind(uint32_t id)
{
    auto it = m_records.find(id);
    if (it == m_records.end())
        return;

    cancel_inbound(it->second);
    cancel_outbound(it->second);
}

// =============================================================================
// Private
// =============================================================================

std::string Bridge::make_task_name(uint32_t id, const char* suffix) const
{
    return "forma_bridge_" + std::to_string(id) + "_" + suffix
        + "_" + std::to_string(m_next_id++);
}

void Bridge::cancel_inbound(ElementRecord& rec)
{
    if (!rec.inbound_task.empty()) {
        m_scheduler.cancel_task(rec.inbound_task);
        rec.inbound_task.clear();
    }
}

void Bridge::cancel_outbound(ElementRecord& rec)
{
    for (const auto& name : rec.outbound_tasks) {
        m_scheduler.cancel_task(name);
    }
    rec.outbound_tasks.clear();
}

void Bridge::spawn_sync(uint32_t id, std::function<void()> sync_fn)
{
    auto name = make_task_name(id, "sync");
    m_records[id].outbound_tasks.push_back(name);

    auto routine = [](Vruta::TaskScheduler&,
                       std::function<void()> fn) -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate) {
            fn();
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(sync_fn))),
        name, false);
}

} // namespace MayaFlux::Portal::Forma
