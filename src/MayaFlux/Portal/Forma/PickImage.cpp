#include "PickImage.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/ImageAccess.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/Spatial/HitTest.hpp"
#include "MayaFlux/Portal/Graphics/ImageTarget.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    constexpr uint32_t k_pick_constant = 8;

    /** Draw @p buffer into @p target writing @p id; false when it cannot be. */
    bool draw_into(
        const std::shared_ptr<Graphics::ImageTarget>& target,
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        uint32_t id)
    {
        const auto processor = buffer ? buffer->get_render_processor() : nullptr;
        if (!processor) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::API,
                "PickImage: the buffer is not set up for rendering");
            return false;
        }

        processor->add_target_image(target, buffer);
        if (!processor->set_destination_shader(target, "forma_pick.frag.spv",
                { { k_pick_constant, std::bit_cast<uint32_t>(static_cast<float>(id)) } })) {
            processor->remove_target_image(target, buffer);
            return false;
        }
        return true;
    }

}

struct PickImage::State {
    struct Sample {
        glm::vec4 value { 0.0F };
        glm::vec2 ndc { 0.0F };
    };

    std::shared_ptr<Graphics::ImageTarget> target;
    std::shared_ptr<Buffers::VKBuffer> staging;
    std::vector<std::pair<uint32_t, std::weak_ptr<Buffers::VKBuffer>>> added;
    std::vector<std::weak_ptr<Buffers::VKBuffer>> occluders;
    uint32_t last_id {};

    Graphics::FenceID pending { Graphics::INVALID_FENCE };
    glm::vec2 pending_ndc { 0.0F };
    Sample sample;

    /** Whether @p buffer was an occluder, which it no longer is. */
    bool drop_occluder(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        return std::erase_if(occluders, [&buffer](const auto& held) { return held.lock() == buffer; }) > 0;
    }

    [[nodiscard]] uint32_t id_of(const std::shared_ptr<Buffers::VKBuffer>& buffer) const
    {
        for (const auto& [id, held] : added) {
            if (held.lock() == buffer)
                return id;
        }
        return 0;
    }

    /** Take in a finished copy, then ask for the texel at ndc if none is in flight. */
    void read(glm::vec2 ndc)
    {
        auto& foundry = Graphics::get_shader_foundry();

        if (pending != Graphics::INVALID_FENCE) {
            if (!foundry.is_fence_signaled(pending))
                return;

            const auto& resources = staging->get_buffer_resources();
            auto service = Registry::BackendRegistry::instance().get_service<Registry::Service::BufferService>();
            if (service && resources.mapped_ptr) {
                service->invalidate_range(resources.memory, 0, sizeof(glm::vec4));
                std::memcpy(&sample.value, resources.mapped_ptr, sizeof(glm::vec4));
                sample.ndc = pending_ndc;
            }
            foundry.release_fence(pending);
            pending = Graphics::INVALID_FENCE;
        }

        const auto image = target->image();
        if (!image || std::abs(ndc.x) > 1.0F || std::abs(ndc.y) > 1.0F)
            return;

        const auto texel = [](float unit, uint32_t size) {
            return static_cast<int32_t>(std::min(
                static_cast<uint32_t>(std::max(unit, 0.0F) * static_cast<float>(size)), size - 1));
        };

        const auto command = foundry.begin_commands(Graphics::ShaderFoundry::CommandBufferType::GRAPHICS);
        const auto cmd = foundry.get_command_buffer(command);
        const vk::ImageSubresourceRange range { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

        Core::record_image_barrier(cmd, image->get_image(), range,
            Core::ImageAccess::SHADER_READ, Core::ImageAccess::TRANSFER_SRC);

        vk::BufferImageCopy region {};
        region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
        region.imageOffset = vk::Offset3D {
            texel(ndc.x * 0.5F + 0.5F, target->width()),
            texel(0.5F - ndc.y * 0.5F, target->height()),
            0
        };
        region.imageExtent = vk::Extent3D { 1, 1, 1 };
        cmd.copyImageToBuffer(image->get_image(), vk::ImageLayout::eTransferSrcOptimal,
            staging->get_buffer(), 1, &region);

        Core::record_image_barrier(cmd, image->get_image(), range,
            Core::ImageAccess::TRANSFER_SRC, Core::ImageAccess::SHADER_READ);

        pending = foundry.submit_async(command);
        pending_ndc = ndc;
    }
};

PickImage::PickImage(uint32_t width, uint32_t height)
    : m_state(std::make_shared<State>())
{
    m_state->target = Graphics::get_texture_manager().create_image_target(
        { .width = width,
            .height = height,
            .format = Graphics::ImageFormat::RGBA32F,
            .depth = true,
            .continuous = true });
    if (!m_state->target) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::Init,
            std::source_location::current(), "PickImage: could not create its image");
    }
    m_state->staging = Buffers::create_staging_buffer(sizeof(glm::vec4));
}

PickImage::~PickImage()
{
    auto held_buffers = m_state->occluders;
    for (const auto& [id, held] : m_state->added)
        held_buffers.push_back(held);

    for (const auto& held : held_buffers) {
        if (const auto buffer = held.lock()) {
            if (const auto processor = buffer->get_render_processor())
                processor->remove_target_image(m_state->target, buffer);
        }
    }

    if (m_state->pending != Graphics::INVALID_FENCE) {
        auto& foundry = Graphics::get_shader_foundry();
        foundry.wait_for_fence(m_state->pending);
        foundry.release_fence(m_state->pending);
        m_state->pending = Graphics::INVALID_FENCE;
    }
}

bool PickImage::add(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (buffer && m_state->id_of(buffer) != 0)
        return true;

    if (buffer && m_state->drop_occluder(buffer)) {
        if (const auto processor = buffer->get_render_processor())
            processor->remove_target_image(m_state->target, buffer);
    }

    const uint32_t id = m_state->last_id + 1;
    if (!draw_into(m_state->target, buffer, id))
        return false;

    m_state->last_id = id;
    m_state->added.emplace_back(id, buffer);
    return true;
}

bool PickImage::occlude(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    const auto is_it = [&buffer](const auto& held) { return held.lock() == buffer; };
    if (buffer && std::ranges::any_of(m_state->occluders, is_it))
        return true;

    remove(buffer);
    if (!draw_into(m_state->target, buffer, 0))
        return false;

    m_state->occluders.emplace_back(buffer);
    return true;
}

void PickImage::remove(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer || (m_state->id_of(buffer) == 0 && !m_state->drop_occluder(buffer)))
        return;

    if (const auto processor = buffer->get_render_processor())
        processor->remove_target_image(m_state->target, buffer);

    std::erase_if(m_state->added, [&buffer](const auto& entry) { return entry.second.lock() == buffer; });
}

std::function<std::optional<glm::vec3>(glm::vec2)> PickImage::to_local(
    const std::shared_ptr<Buffers::VKBuffer>& buffer,
    std::function<Kinesis::ViewTransform()> view,
    Kinesis::AABB2D bounds) const
{
    return [weak = std::weak_ptr<State>(m_state), id = m_state->id_of(buffer),
               view = std::move(view), bounds](glm::vec2 screen) -> std::optional<glm::vec3> {
        const auto state = weak.lock();
        if (!state || id == 0)
            return std::nullopt;

        state->read(screen);

        const auto& [value, ndc] = state->sample;
        if (static_cast<uint32_t>(value.x) != id)
            return std::nullopt;

        const Kinesis::ViewTransform vt = view();
        const glm::vec4 met = glm::inverse(vt.projection * vt.view) * glm::vec4(ndc, value.w, 1.0F);
        const float distance = glm::distance(glm::vec3(met) / met.w, Kinesis::ray_through(ndc, vt).origin);

        const glm::vec2 size = bounds.max - bounds.min;
        return glm::vec3(bounds.min.x + value.y * size.x, bounds.max.y - value.z * size.y, distance);
    };
}

const std::shared_ptr<Graphics::ImageTarget>& PickImage::image() const noexcept
{
    return m_state->target;
}

} // namespace MayaFlux::Portal::Forma
