#include "Attachment.hpp"

#include "MayaFlux/Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryBuffer.hpp"
#include "MayaFlux/Buffers/Geometry/MeshBuffer.hpp"
#include "MayaFlux/Buffers/Network/MeshNetworkBuffer.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Kinesis/Morphology.hpp"
#include "MayaFlux/Nodes/Graphics/MeshWriterNode.hpp"
#include "MayaFlux/Nodes/Network/MeshNetwork.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace MayaFlux::Nexus {

namespace {

    glm::quat frame(const std::optional<glm::quat>& orientation)
    {
        return orientation.value_or(glm::quat(1.0F, 0.0F, 0.0F, 0.0F));
    }

    std::optional<Kakshya::MeshData> snapshot_mesh(const std::shared_ptr<Buffers::VKBuffer>& buffer)
    {
        if (const auto mesh = std::dynamic_pointer_cast<Buffers::MeshBuffer>(buffer)) {
            return mesh->get_mesh_data();
        }

        if (const auto network_buffer = std::dynamic_pointer_cast<Buffers::MeshNetworkBuffer>(buffer)) {
            const auto network = network_buffer->get_network();
            return network ? network->get_mesh_data() : std::nullopt;
        }

        if (const auto geometry = std::dynamic_pointer_cast<Buffers::GeometryBuffer>(buffer)) {
            const auto node = std::dynamic_pointer_cast<Nodes::GpuSync::MeshWriterNode>(
                geometry->get_geometry_node());
            return node ? node->get_mesh_data() : std::nullopt;
        }

        if (const auto compute = std::dynamic_pointer_cast<Buffers::ComputeMeshBuffer>(buffer)) {
            return Buffers::download_compute_mesh(compute);
        }

        return std::nullopt;
    }

    size_t position_offset(const Kakshya::VertexLayout& layout)
    {
        const auto it = std::ranges::find_if(layout.attributes,
            [](const auto& attribute) {
                return attribute.component_modality == Kakshya::DataModality::VERTEX_POSITIONS_3D;
            });
        return it != layout.attributes.end() ? it->offset_in_vertex : 0;
    }

    std::optional<glm::vec3> read_anchor(
        const std::shared_ptr<Buffers::VKBuffer>& buf,
        const std::optional<uint32_t>& index)
    {
        const auto mesh = snapshot_mesh(buf);
        if (!mesh) {
            return std::nullopt;
        }

        const auto* vertices = std::get_if<std::vector<uint8_t>>(&mesh->vertex_variant);
        if (!vertices || vertices->empty()) {
            return std::nullopt;
        }

        const auto bytes = std::as_bytes(std::span(*vertices));
        const size_t stride = mesh->layout.stride_bytes;
        const size_t offset = position_offset(mesh->layout);

        if (index) {
            return Kinesis::position_at(bytes, stride, offset, *index);
        }

        if (const auto* faces = std::get_if<std::vector<uint32_t>>(&mesh->index_variant);
            faces && !faces->empty()) {
            return Kinesis::surface_centroid(bytes, stride, offset, *faces);
        }
        return Kinesis::centroid(bytes, stride, offset);
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
        const bool known = std::ranges::find(attachment.followed, proc) != attachment.followed.end();
        if (known && !proc->get_view_transform()) {
            return;
        }

        std::function<Kinesis::ViewTransform()> base = proc->get_view_transform_source();
        if (!base) {
            if (const auto& vt = proc->get_view_transform()) {
                base = [v = *vt] { return v; };
            }
        }

        const bool scene = static_cast<bool>(base);

        proc->set_view_transform_source(
            [base = std::move(base), transform = attachment.transform] {
                Kinesis::ViewTransform vt = base ? base() : Kinesis::ViewTransform {};
                vt.view = vt.view * *transform;
                return vt;
            },
            scene,
            scene ? Portal::Graphics::CullMode::BACK : Portal::Graphics::CullMode::NONE);

        if (!known) {
            attachment.followed.push_back(proc);
        }
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

    for (const auto& proc : render_processors(attachment.buf)) {
        place(attachment, proc);
    }
}

void release_attachment(Attachment& attachment)
{
    *attachment.transform = glm::mat4(1.0F);
}

} // namespace MayaFlux::Nexus
