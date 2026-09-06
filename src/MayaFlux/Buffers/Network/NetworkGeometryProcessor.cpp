#include "NetworkGeometryProcessor.hpp"

#include "NetworkGeometryBuffer.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NetworkGeometryProcessor::NetworkGeometryProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

void NetworkGeometryProcessor::bind_network(
    const std::string& name,
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    const std::shared_ptr<VKBuffer>& vertex_buffer)
{
    if (!network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind null network '{}'", name);
    }

    if (!vertex_buffer) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind network '{}' to null vertex buffer", name);
    }

    std::shared_ptr<VKBuffer> staging = nullptr;
    if (!vertex_buffer->is_host_visible()) {
        staging = create_staging_buffer(vertex_buffer->get_size_bytes());

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Created staging buffer for device-local network geometry '{}' ({} bytes)",
            name, vertex_buffer->get_size_bytes());
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No staging needed for host-visible network geometry '{}'", name);
    }

    m_bindings[name] = NetworkBinding {
        .network = network,
        .gpu_vertex_buffer = vertex_buffer,
        .staging_buffer = staging
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound network '{}' ({} nodes, {} bytes buffer)",
        name, network->get_node_count(), vertex_buffer->get_size_bytes());
}

void NetworkGeometryProcessor::unbind_network(const std::string& name)
{
    if (m_bindings.erase(name) == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to unbind non-existent network '{}'", name);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Unbound network '{}'", name);
    }
}

bool NetworkGeometryProcessor::has_binding(const std::string& name) const
{
    return m_bindings.contains(name);
}

std::vector<std::string> NetworkGeometryProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

size_t NetworkGeometryProcessor::get_binding_count() const
{
    return m_bindings.size();
}

std::optional<NetworkGeometryProcessor::NetworkBinding>
NetworkGeometryProcessor::get_binding(const std::string& name) const
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        return it->second;
    }
    return std::nullopt;
}

void NetworkGeometryProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_bindings.empty())
        return;

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NetworkGeometryProcessor requires VKBuffer");
        return;
    }

    for (auto& [name, binding] : m_bindings) {
        if (!binding.network || !binding.network->is_enabled()) {
            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Network '{}' disabled, skipping upload", name);
            continue;
        }

        const auto* chain = binding.network->get_operator_chain().get();
        const bool has_chain = chain && !chain->empty();

        if (!has_chain) {
            auto gpu_data = extract_network_gpu_data(binding.network, name);
            if (gpu_data.vertex_data.empty() || gpu_data.vertex_count == 0) {
                if (vk_buffer->is_host_visible())
                    vk_buffer->clear();
                continue;
            }

            size_t total = gpu_data.vertex_data.size();
            if (total > vk_buffer->get_size_bytes()) {
                vk_buffer->resize(static_cast<size_t>(static_cast<float>(total) * 1.5F), false);
            }

            ensure_staging(binding, total);
            upload_to_gpu(gpu_data.vertex_data.data(), total, vk_buffer, binding.staging_buffer);

            if (gpu_data.layout)
                vk_buffer->set_vertex_layout(*gpu_data.layout);

            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Uploaded {} vertices from network '{}' ({} bytes)",
                gpu_data.vertex_count, name, total);
            continue;
        }

        struct SliceData {
            std::span<const uint8_t> bytes;
            size_t vertex_count {};
            std::optional<Kakshya::VertexLayout> layout;
        };

        std::vector<SliceData> slices;
        slices.reserve(1 + chain->size());

        auto primary = extract_network_gpu_data(binding.network, name);
        slices.push_back({ .bytes = primary.vertex_data, .vertex_count = primary.vertex_count, .layout = primary.layout });

        for (const auto& op : chain->operators()) {
            auto* gfx = dynamic_cast<Nodes::Network::GraphicsOperator*>(op.get());
            if (!gfx) {
                slices.push_back({ .bytes = {}, .vertex_count = 0, .layout = std::nullopt });
                continue;
            }

            if (!gfx->participates_in_rendering())
                continue;

            slices.push_back({ .bytes = gfx->get_vertex_data(), .vertex_count = gfx->get_vertex_count(), .layout = gfx->get_vertex_layout() });
        }

        size_t total_bytes = 0;
        for (const auto& s : slices)
            total_bytes += s.bytes.size();

        if (total_bytes == 0) {
            if (vk_buffer->is_host_visible())
                vk_buffer->clear();
            continue;
        }

        auto* primary_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(
            binding.network->get_operator());

        bool chain_gfx_dirty = false;
        bool needs_full_restore = false;
        for (const auto& op : chain->operators()) {
            if (op && op->demands_full_vertex_restore())
                needs_full_restore = true;

            auto* gfx = dynamic_cast<Nodes::Network::GraphicsOperator*>(op.get());
            if (gfx && gfx->participates_in_rendering() && gfx->is_vertex_data_dirty())
                chain_gfx_dirty = true;
        }

        if (primary_op && primary_op->supports_incremental_upload()
            && !needs_full_restore
            && total_bytes == binding.last_total_bytes
            && !chain_gfx_dirty && primary.layout.has_value()) {

            const auto ranges = primary_op->dirty_vertex_ranges();
            const uint32_t stride = primary.layout->stride_bytes;
            bool applied = stride > 0;

            for (const auto& r : ranges) {
                const auto pack = primary_op->get_vertex_data_for_collection(r.group_index);
                const size_t offset = static_cast<size_t>(r.vertex_offset) * stride;
                const size_t length = static_cast<size_t>(r.vertex_count) * stride;
                if (length == 0)
                    continue;
                if (pack.size() < length || offset + length > vk_buffer->get_size_bytes()) {
                    applied = false;
                    break;
                }
                ensure_staging(binding, length);
                upload_to_gpu(pack.data(), length, vk_buffer, binding.staging_buffer, offset);
            }

            if (applied) {
                primary_op->mark_vertex_data_clean();
                MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Network '{}': incremental upload, {} dirty range(s)", name, ranges.size());
                continue;
            }
        }

        if (total_bytes > vk_buffer->get_size_bytes()) {
            vk_buffer->resize(static_cast<size_t>(static_cast<float>(total_bytes) * 1.5F), false);
        }

        m_staging_aggregate.resize(total_bytes);
        size_t byte_offset = 0;
        for (const auto& s : slices) {
            if (!s.bytes.empty())
                std::memcpy(m_staging_aggregate.data() + byte_offset, s.bytes.data(), s.bytes.size());
            byte_offset += s.bytes.size();
        }

        ensure_staging(binding, total_bytes);
        upload_to_gpu(m_staging_aggregate.data(), total_bytes, vk_buffer, binding.staging_buffer);

        for (auto& slice : std::views::reverse(slices)) {
            if (slice.layout) {
                vk_buffer->set_vertex_layout(*slice.layout);
                break;
            }
        }

        auto ngb = std::dynamic_pointer_cast<NetworkGeometryBuffer>(buffer);
        if (ngb) {
            byte_offset = 0;
            for (size_t i = 0; i < slices.size(); ++i) {
                const auto& s = slices[i];
                const uint32_t stride = s.layout ? s.layout->stride_bytes
                                                 : (slices[0].layout ? slices[0].layout->stride_bytes : 0);
                const uint32_t vert_offset = stride > 0
                    ? static_cast<uint32_t>(byte_offset / stride)
                    : 0;

                ngb->update_chain_render_range(i, vert_offset,
                    static_cast<uint32_t>(s.vertex_count), s.layout);

                byte_offset += s.bytes.size();
            }
        }

        binding.last_total_bytes = total_bytes;

        MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Uploaded {} bytes ({} slices) from network '{}'",
            total_bytes, slices.size(), name);
    }
}

void NetworkGeometryProcessor::ensure_staging(NetworkBinding& binding, size_t bytes)
{
    if (!binding.gpu_vertex_buffer || binding.gpu_vertex_buffer->is_host_visible())
        return;

    if (binding.staging_buffer && binding.staging_buffer->get_size_bytes() >= bytes)
        return;

    const auto grown = static_cast<size_t>(static_cast<float>(bytes) * 1.5F);
    binding.staging_buffer = create_staging_buffer(grown);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "NetworkGeometryProcessor: staging buffer grown to {} bytes", grown);
}

} // namespace MayaFlux::Buffers
