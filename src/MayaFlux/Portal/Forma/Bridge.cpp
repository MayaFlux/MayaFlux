#include "Bridge.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Forma/FormaBindingsProcessor.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"
#include "MayaFlux/Buffers/Staging/DataWriteProcessor.hpp"

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
    for (auto& [layer, records] : m_records) {
        for (auto& [id, rec] : records) {
            cancel_inbound(rec);
            cancel_outbound(rec);
        }
    }

    for (auto& [layer, tasks] : m_layer_tasks) {
        for (const auto& name : tasks)
            m_scheduler.cancel_task(name);
    }
}

// =============================================================================
// Inbound
// =============================================================================

void Bridge::bind(
    const ElementRef& ref,
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

    spawn_inbound(ref, std::move(reader));
}

void Bridge::bind(const ElementRef& ref, std::function<float()> source)
{
    spawn_inbound(ref, std::move(source));
}

void Bridge::spawn_inbound(const ElementRef& ref, std::function<float()> source)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::bind: unknown element id {}", ref.second);
        return;
    }

    cancel_inbound(*rec);

    auto name = make_task_name(ref.second, "inbound");
    rec->inbound_task = name;

    auto writer = rec->writer;

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
    const ElementRef& ref,
    const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
    const std::string& shader_path,
    uint32_t offset,
    size_t size)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", ref.second);
        return;
    }

    if (!rec->bindings) {
        rec->bindings = std::make_shared<Buffers::FormaBindingsProcessor>(shader_path);
        m_buffer_manager.add_processor(rec->bindings, target_buffer,
            Buffers::ProcessingToken::GRAPHICS_BACKEND);
    }

    auto binding_name = std::to_string(ref.second) + "_pc_" + std::to_string(offset);
    rec->bindings->bind_push_constant(binding_name, rec->reader, offset, size);
    rec->binding_names.push_back(std::move(binding_name));
}

void Bridge::write(
    const ElementRef& ref,
    const std::shared_ptr<Buffers::VKBuffer>& target_buffer,
    const std::string& shader_path,
    const std::string& descriptor_name,
    uint32_t binding_index,
    uint32_t set,
    Portal::Graphics::DescriptorRole role)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", ref.second);
        return;
    }

    if (!rec->bindings) {
        rec->bindings = std::make_shared<Buffers::FormaBindingsProcessor>(shader_path);
        m_buffer_manager.add_processor(rec->bindings, target_buffer,
            Buffers::ProcessingToken::GRAPHICS_BACKEND);
    }

    auto binding_name = descriptor_name + "_" + std::to_string(ref.second);
    rec->bindings->bind_descriptor(binding_name, rec->reader, descriptor_name, binding_index, set, role);
    rec->binding_names.push_back(std::move(binding_name));
}

void Bridge::write(const ElementRef& ref, std::shared_ptr<Buffers::AudioWriteProcessor> target)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", ref.second);
        return;
    }

    auto name = make_task_name(ref.second, "audio");
    rec->outbound_tasks.push_back(name);

    auto bulk = rec->bulk_reader;
    auto reader = rec->reader;

    if (bulk) {
        auto routine = [](Vruta::TaskScheduler&,
                           std::function<std::vector<float>()> b,
                           std::shared_ptr<Buffers::AudioWriteProcessor> proc)
            -> Vruta::GraphicsRoutine {
            auto& p = co_await Kriya::GetGraphicsPromise {};
            while (!p.should_terminate) {
                proc->set_data(std::span<const float>(b()));
                co_await Kriya::FrameDelay { .frames_to_wait = 1 };
            }
        };
        m_scheduler.add_task(
            std::make_shared<Vruta::GraphicsRoutine>(
                routine(m_scheduler, std::move(bulk), std::move(target))),
            name, false);
    } else {
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
}

void Bridge::write(const ElementRef& ref, std::shared_ptr<Buffers::DataWriteProcessor> target)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", ref.second);
        return;
    }

    auto name = make_task_name(ref.second, "geometry");
    rec->outbound_tasks.push_back(name);

    auto bulk = rec->bulk_reader;
    auto reader = rec->reader;

    if (bulk) {
        auto routine = [](Vruta::TaskScheduler&,
                           std::function<std::vector<float>()> b,
                           std::shared_ptr<Buffers::DataWriteProcessor> proc)
            -> Vruta::GraphicsRoutine {
            auto& p = co_await Kriya::GetGraphicsPromise {};
            while (!p.should_terminate) {
                proc->set_data(Kakshya::DataVariant { b() });
                co_await Kriya::FrameDelay { .frames_to_wait = 1 };
            }
        };
        m_scheduler.add_task(
            std::make_shared<Vruta::GraphicsRoutine>(
                routine(m_scheduler, std::move(bulk), std::move(target))),
            name, false);
    } else {
        auto routine = [](Vruta::TaskScheduler&,
                           std::function<float()> r,
                           std::shared_ptr<Buffers::DataWriteProcessor> proc)
            -> Vruta::GraphicsRoutine {
            auto& p = co_await Kriya::GetGraphicsPromise {};
            while (!p.should_terminate) {
                proc->set_data(std::vector<Kakshya::DataVariant> { Kakshya::DataVariant { std::vector<float> { r() } } });
                co_await Kriya::FrameDelay { .frames_to_wait = 1 };
            }
        };
        m_scheduler.add_task(
            std::make_shared<Vruta::GraphicsRoutine>(
                routine(m_scheduler, std::move(reader), std::move(target))),
            name, false);
    }
}

void Bridge::write(const ElementRef& ref, std::shared_ptr<Nodes::Constant> node)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", ref.second);
        return;
    }

    auto name = make_task_name(ref.second, "constant");
    rec->outbound_tasks.push_back(name);

    auto reader = rec->reader;

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

void Bridge::write(const ElementRef& ref, std::function<void(std::span<const float>)> sink)
{
    auto* rec = find_record(ref);
    if (!rec) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Init,
            "Bridge::write: unknown element id {}", ref.second);
        return;
    }

    auto name = make_task_name(ref.second, "bulk_sink");
    rec->outbound_tasks.push_back(name);

    auto bulk = rec->bulk_reader;
    auto reader = rec->reader;

    if (bulk) {
        auto routine = [](Vruta::TaskScheduler&,
                           std::function<std::vector<float>()> b,
                           std::function<void(std::span<const float>)> s)
            -> Vruta::GraphicsRoutine {
            auto& p = co_await Kriya::GetGraphicsPromise {};
            while (!p.should_terminate) {
                const auto v = b();
                s(v);
                co_await Kriya::FrameDelay { .frames_to_wait = 1 };
            }
        };
        m_scheduler.add_task(
            std::make_shared<Vruta::GraphicsRoutine>(
                routine(m_scheduler, std::move(bulk), std::move(sink))),
            name, false);
    } else {
        auto routine = [](Vruta::TaskScheduler&,
                           std::function<float()> r,
                           std::function<void(std::span<const float>)> s)
            -> Vruta::GraphicsRoutine {
            auto& p = co_await Kriya::GetGraphicsPromise {};
            float val {};
            while (!p.should_terminate) {
                val = r();
                s(std::span<const float> { &val, 1 });
                co_await Kriya::FrameDelay { .frames_to_wait = 1 };
            }
        };
        m_scheduler.add_task(
            std::make_shared<Vruta::GraphicsRoutine>(
                routine(m_scheduler, std::move(reader), std::move(sink))),
            name, false);
    }
}

void Bridge::write(
    Layer& layer,
    const std::vector<std::shared_ptr<MappedState<float>>>& states,
    std::function<void(std::span<const float>)> sink)
{
    std::vector<std::function<float()>> readers;
    readers.reserve(states.size());
    for (const auto& state : states) {
        readers.emplace_back([state] {
            return state->value;
        });
    }
    write_aggregate(layer, std::move(readers), std::move(sink));
}

// =============================================================================
// Lifecycle
// =============================================================================

void Bridge::unbind(const ElementRef& ref)
{
    auto* rec = find_record(ref);
    if (!rec)
        return;

    cancel_inbound(*rec);
    cancel_outbound(*rec);
}

void Bridge::stop_sync(Layer& layer)
{
    auto it = m_layer_tasks.find(layer.weak_from_this());
    if (it == m_layer_tasks.end())
        return;

    for (const auto& name : it->second)
        m_scheduler.cancel_task(name);

    m_layer_tasks.erase(it);
}

// =============================================================================
// Private
// =============================================================================

Bridge::ElementRecord* Bridge::find_record(const ElementRef& ref)
{
    const auto layer_it = m_records.find(ref.first);
    if (layer_it == m_records.end())
        return nullptr;

    const auto it = layer_it->second.find(ref.second);
    return it == layer_it->second.end() ? nullptr : &it->second;
}

std::string Bridge::make_task_name(uint32_t id, const char* suffix) const
{
    return "forma_bridge_" + std::to_string(id) + "_" + suffix
        + "_" + std::to_string(m_next_id++);
}

std::string Bridge::make_layer_task_name(const char* suffix) const
{
    return "forma_bridge_layer_" + std::string(suffix)
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

    if (rec.bindings) {
        for (const auto& name : rec.binding_names)
            rec.bindings->unbind(name);
    }
    rec.binding_names.clear();
}

void Bridge::spawn_sync(Layer& layer, uint32_t id, std::function<void()> sync_fn)
{
    auto name = make_task_name(id, "sync");
    const ElementRef ref { layer.weak_from_this(), id };
    m_layer_tasks[ref.first].push_back(name);
    if (auto* rec = find_record(ref))
        rec->outbound_tasks.push_back(name);

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

void Bridge::write_aggregate(
    Layer& layer,
    std::vector<std::function<float()>> readers,
    std::function<void(std::span<const float>)> sink)
{
    auto name = make_layer_task_name("bulk_sink");
    m_layer_tasks[layer.weak_from_this()].push_back(name);

    auto routine = [](Vruta::TaskScheduler&,
                       std::vector<std::function<float()>> sources,
                       std::function<void(std::span<const float>)> target)
        -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        std::vector<float> values(sources.size());
        while (!p.should_terminate) {
            for (size_t i = 0; i < sources.size(); ++i)
                values[i] = sources[i]();
            target(values);
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(readers), std::move(sink))),
        name, false);
}

void Bridge::write_assembled_data(
    Layer& layer,
    std::function<Kakshya::DataVariant()> assemble,
    std::function<void(Kakshya::DataVariant)> write)
{
    auto name = make_layer_task_name("assembled_data");
    m_layer_tasks[layer.weak_from_this()].push_back(name);

    auto routine = [](Vruta::TaskScheduler&,
                       std::function<Kakshya::DataVariant()> make_data,
                       std::function<void(Kakshya::DataVariant)> target)
        -> Vruta::GraphicsRoutine {
        auto& p = co_await Kriya::GetGraphicsPromise {};
        while (!p.should_terminate) {
            target(make_data());
            co_await Kriya::FrameDelay { .frames_to_wait = 1 };
        }
    };

    m_scheduler.add_task(
        std::make_shared<Vruta::GraphicsRoutine>(
            routine(m_scheduler, std::move(assemble), std::move(write))),
        name, false);
}

} // namespace MayaFlux::Portal::Forma
