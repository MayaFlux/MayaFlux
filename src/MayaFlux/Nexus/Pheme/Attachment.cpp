#include "Attachment.hpp"

#include "Survey.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Kinesis/Morphology.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace MayaFlux::Nexus {

struct PlacementStack {
    std::function<glm::mat4()> base;
    std::vector<std::shared_ptr<glm::mat4>> layers;
};

namespace {

    struct StackedSource {
        std::shared_ptr<PlacementStack> stack;

        glm::mat4 operator()() const
        {
            glm::mat4 placed = stack->base ? stack->base() : glm::mat4(1.0F);
            for (const auto& layer : stack->layers) {
                placed = *layer * placed;
            }
            return placed;
        }
    };

    std::shared_ptr<PlacementStack> stack_of(const std::shared_ptr<Buffers::RenderProcessor>& proc)
    {
        const auto& source = proc->get_geometry_transform_source();
        const auto* stacked = source ? source.target<StackedSource>() : nullptr;
        return stacked ? stacked->stack : nullptr;
    }

    glm::quat frame(const std::optional<glm::quat>& orientation)
    {
        return orientation.value_or(glm::quat(1.0F, 0.0F, 0.0F, 0.0F));
    }

    std::vector<std::shared_ptr<Buffers::RenderProcessor>> render_processors(
        const std::shared_ptr<Buffers::VKBuffer>& buf)
    {
        std::vector<std::shared_ptr<Buffers::RenderProcessor>> processors;
        if (auto primary = buf->get_render_processor()) {
            processors.push_back(std::move(primary));
        }
        for (auto& proc : buf->get_additional_render_processors()) {
            processors.push_back(std::move(proc));
        }
        return processors;
    }

    void place(Attachment& attachment, const std::shared_ptr<Buffers::RenderProcessor>& proc)
    {
        const bool known = std::ranges::any_of(attachment.followed,
            [&proc](const auto& entry) { return entry.first == proc; });
        if (known) {
            return;
        }

        auto stack = stack_of(proc);
        if (!stack) {
            stack = std::make_shared<PlacementStack>();
            stack->base = proc->get_geometry_transform_source();
            if (!stack->base) {
                if (const auto& fixed = proc->get_geometry_transform()) {
                    stack->base = [m = *fixed] { return m; };
                }
            }
            proc->set_geometry_transform_source(StackedSource { stack });
        }

        stack->layers.push_back(attachment.transform);
        attachment.followed.emplace_back(proc, std::move(stack));
    }

}

Attachment make_attachment(
    std::shared_ptr<Buffers::VKBuffer> buf,
    const AttachConfig& config,
    const std::optional<glm::quat>& orientation)
{
    Attachment attachment {
        .buf = std::move(buf),
        .config = config,
        .transform = std::make_shared<glm::mat4>(1.0F),
    };

    if (const auto anchor = read_anchor(attachment.buf, config.index)) {
        attachment.anchor = *anchor;
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Init,
            "Attachment anchors at the buffer's local origin: no geometry could be read from it");
    }

    attachment.applied = attachment.anchor + frame(orientation) * config.offset;
    return attachment;
}

void follow_attachment(
    Attachment& attachment,
    std::optional<glm::vec3>& position,
    const std::optional<glm::quat>& orientation)
{
    const glm::quat rotation = frame(orientation);
    glm::vec3 placed = glm::vec3(*attachment.transform * glm::vec4(attachment.anchor, 1.0F));

    if (position && *position != attachment.applied) {
        const glm::vec3 delta = *position - attachment.applied;
        if (attachment.config.on_move == AttachConfig::OnMove::Carry) {
            *attachment.transform = glm::translate(glm::mat4(1.0F), delta) * *attachment.transform;
            placed += delta;
        } else {
            attachment.config.offset += glm::inverse(rotation) * delta;
        }
    }

    position = placed + rotation * attachment.config.offset;
    attachment.applied = *position;
}

void recenter_attachment(
    Attachment& attachment,
    std::optional<glm::vec3>& position,
    const std::optional<glm::quat>& orientation)
{
    const glm::quat rotation = frame(orientation);

    if (const auto anchor = read_anchor(attachment.buf, attachment.config.index)) {
        attachment.anchor = *anchor;
    } else {
        MF_WARN(Journal::Component::Nexus, Journal::Context::Runtime,
            "recenter kept the previous anchor: no geometry could be read from the buffer");
    }

    const glm::vec3 placed = glm::vec3(*attachment.transform * glm::vec4(attachment.anchor, 1.0F));

    if (position) {
        attachment.config.offset = glm::inverse(rotation) * (*position - placed);
    } else {
        position = placed + rotation * attachment.config.offset;
    }
    attachment.applied = *position;
}

void apply_attachment(Attachment& attachment, const InfluenceContext& ctx)
{
    const glm::quat rotation = frame(ctx.orientation);

    *attachment.transform = Kinesis::pivot_transform(
        attachment.anchor,
        ctx.position - rotation * attachment.config.offset,
        rotation);

    place_attachment(attachment);
}

void place_attachment(Attachment& attachment)
{
    for (const auto& proc : render_processors(attachment.buf)) {
        place(attachment, proc);
    }
}

void release_attachment(Attachment& attachment)
{
    for (auto& [proc, stack] : attachment.followed) {
        std::erase(stack->layers, attachment.transform);
        if (stack->layers.empty() && stack_of(proc) == stack) {
            proc->set_geometry_transform_source(std::move(stack->base));
        }
    }
    attachment.followed.clear();
    *attachment.transform = glm::mat4(1.0F);
}

} // namespace MayaFlux::Nexus
