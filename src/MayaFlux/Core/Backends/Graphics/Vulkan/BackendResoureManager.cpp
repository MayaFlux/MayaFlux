#include "BackendResoureManager.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKCommandManager.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Core {

namespace {
    struct FencedSubmission {
        vk::CommandBuffer cmd;
        TimelinePoint point {};
    };

    void record_barrier(
        vk::CommandBuffer cmd,
        vk::ImageMemoryBarrier2& barrier,
        vk::PipelineStageFlags2 src_stage,
        vk::PipelineStageFlags2 dst_stage)
    {
        barrier.srcStageMask = src_stage;
        barrier.dstStageMask = dst_stage;

        vk::DependencyInfo dependency {};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        cmd.pipelineBarrier2(dependency);
    }
}

BackendResourceManager::BackendResourceManager(VKContext& context, VKCommandManager& command_manager)
    : m_context(context)
    , m_command_manager(command_manager)
{
}

void BackendResourceManager::setup_backend_service(const std::shared_ptr<Registry::Service::BufferService>& buffer_service)
{
    buffer_service->initialize_buffer = [this](const std::shared_ptr<void>& vk_buf) -> void {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->initialize_buffer(buffer);
    };

    buffer_service->allocate_raw_buffer = [this](
                                              size_t size_bytes,
                                              uint32_t usage_flags,
                                              uint32_t memory_property_flags,
                                              bool host_visible,
                                              void*& out_buffer,
                                              void*& out_memory,
                                              void*& out_mapped_ptr) -> void {
        vk::Buffer buf;
        vk::DeviceMemory mem;
        void* mapped = nullptr;
        this->allocate_raw_buffer(
            size_bytes,
            vk::BufferUsageFlags(usage_flags),
            vk::MemoryPropertyFlags(memory_property_flags),
            host_visible,
            buf, mem, mapped);
        out_buffer = static_cast<VkBuffer>(buf);
        out_memory = static_cast<VkDeviceMemory>(mem);
        out_mapped_ptr = mapped;
    };

    buffer_service->destroy_buffer = [this](const std::shared_ptr<void>& vk_buf) {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->cleanup_buffer(buffer);
    };

    buffer_service->retire_buffer = [this](const std::shared_ptr<void>& vk_buf) {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        this->retire_buffer(buffer);
    };

    buffer_service->get_buffer_device_address = [this](const std::shared_ptr<void>& vk_buf) -> uint64_t {
        auto buffer = std::static_pointer_cast<Buffers::VKBuffer>(vk_buf);
        auto address = this->get_buffer_device_address(buffer);
        return static_cast<uint64_t>(address);
    };

    buffer_service->execute_immediate = [this](const std::function<void(void*)>& recorder) {
        this->execute_immediate_commands([recorder](vk::CommandBuffer cmd) {
            recorder(static_cast<void*>(cmd));
        });
    };

    buffer_service->record_deferred = [this](const std::function<void(void*)>& recorder) {
        this->record_deferred_commands([recorder](vk::CommandBuffer cmd) {
            recorder(static_cast<void*>(cmd));
        });
    };

    buffer_service->flush_range = [this](void* memory, size_t offset, size_t size) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        auto range = mapped_memory_range(mem, offset, size);
        if (auto result = m_context.get_device().flushMappedMemoryRanges(1, &range); result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to flush mapped memory range: {}", vk::to_string(result));
        }
    };

    buffer_service->invalidate_range = [this](void* memory, size_t offset, size_t size) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        auto range = mapped_memory_range(mem, offset, size);
        if (auto result = m_context.get_device().invalidateMappedMemoryRanges(1, &range); result != vk::Result::eSuccess) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to invalidate mapped memory range: {}", vk::to_string(result));
        }
    };

    buffer_service->map_buffer = [this](void* memory, size_t offset, size_t size) -> void* {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        return m_context.get_device().mapMemory(mem, offset, size == 0 ? VK_WHOLE_SIZE : size);
    };

    buffer_service->unmap_buffer = [this](void* memory) {
        vk::DeviceMemory mem(reinterpret_cast<VkDeviceMemory>(memory));
        m_context.get_device().unmapMemory(mem);
    };

    buffer_service->copy_buffer = [this](void* src, void* dst, size_t size, size_t src_off, size_t dst_off) {
        vk::Buffer s(static_cast<VkBuffer>(src));
        vk::Buffer d(static_cast<VkBuffer>(dst));
        execute_immediate_commands([&](vk::CommandBuffer cmd) {
            vk::BufferCopy region { src_off, dst_off, size };
            cmd.copyBuffer(s, d, 1, &region);
        });
    };

    buffer_service->execute_fenced = [this](const std::function<void(void*)>& recorder)
        -> std::shared_ptr<void> {
        vk::CommandBuffer cmd = m_command_manager.begin_single_time_commands();

        recorder(static_cast<void*>(cmd));

        cmd.end();

        const vk::CommandBuffer commands[] = { cmd };
        const auto waits = m_command_manager.produced_waits(vk::PipelineStageFlagBits2::eAllCommands);
        const TimelinePoint point = m_command_manager.submit(m_context.get_graphics_queue(), commands, waits);
        if (!point) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "execute_fenced: queue submit failed");
            m_command_manager.free_command_buffer(cmd);
            return nullptr;
        }

        auto handle = std::make_shared<FencedSubmission>();
        handle->cmd = cmd;
        handle->point = point;
        return handle;
    };

    buffer_service->wait_fenced = [this](const std::shared_ptr<void>& handle) {
        if (!handle)
            return;
        auto sub = std::static_pointer_cast<FencedSubmission>(handle);
        m_command_manager.wait(sub->point);
    };

    buffer_service->release_fenced = [this](const std::shared_ptr<void>& handle) {
        if (!handle)
            return;
        auto sub = std::static_pointer_cast<FencedSubmission>(handle);

        if (sub->cmd) {
            m_command_manager.retire(sub->cmd, sub->point);
            sub->cmd = nullptr;
        }
    };

    buffer_service->copy_buffer_fenced = [this, buffer_service](void* src, void* dst, size_t size, size_t src_off, size_t dst_off)
        -> std::shared_ptr<void> {
        vk::Buffer s(static_cast<VkBuffer>(src));
        vk::Buffer d(static_cast<VkBuffer>(dst));
        return buffer_service->execute_fenced([&](void* cmd_ptr) {
            vk::CommandBuffer cmd(static_cast<VkCommandBuffer>(cmd_ptr));
            vk::BufferCopy region { src_off, dst_off, static_cast<vk::DeviceSize>(size) };
            cmd.copyBuffer(s, d, 1, &region);
        });
    };

    buffer_service->await_dispatches = [this]() {
        const auto produced = m_command_manager.produced();
        if (!m_command_manager.is_complete(produced)) {
            m_command_manager.wait(produced);
        }
    };
}

vk::MappedMemoryRange BackendResourceManager::mapped_memory_range(
    vk::DeviceMemory memory,
    vk::DeviceSize offset,
    vk::DeviceSize size) const
{
    vk::MappedMemoryRange range {};
    range.memory = memory;

    const auto allocation = m_buffer_allocation_sizes.find(static_cast<VkDeviceMemory>(memory));
    if (allocation == m_buffer_allocation_sizes.end()) {
        range.offset = offset;
        range.size = size == 0 ? VK_WHOLE_SIZE : size;
        return range;
    }

    const vk::DeviceSize allocation_size = allocation->second;
    const vk::DeviceSize atom_size = std::max<vk::DeviceSize>(
        1, m_context.get_physical_device().getProperties().limits.nonCoherentAtomSize);
    const vk::DeviceSize start = std::min(offset, allocation_size);
    const vk::DeviceSize requested_end = size == 0 || size == VK_WHOLE_SIZE
        ? allocation_size
        : start + std::min(size, allocation_size - start);
    const vk::DeviceSize aligned_offset = (start / atom_size) * atom_size;
    const vk::DeviceSize rounded_end = requested_end > allocation_size - std::min(atom_size - 1, allocation_size)
        ? allocation_size
        : ((requested_end + atom_size - 1) / atom_size) * atom_size;

    range.offset = aligned_offset;
    range.size = rounded_end == allocation_size
        ? VK_WHOLE_SIZE
        : rounded_end - aligned_offset;
    return range;
}

void BackendResourceManager::allocate_raw_buffer(
    size_t size_bytes,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags memory_properties,
    bool host_visible,
    vk::Buffer& out_buffer,
    vk::DeviceMemory& out_memory,
    void*& out_mapped_ptr)
{
    vk::BufferCreateInfo buffer_info {};
    buffer_info.size = size_bytes;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    try {
        out_buffer = m_context.get_device().createBuffer(buffer_info);
    } catch (const vk::SystemError& e) {
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create raw VkBuffer: " + std::string(e.what()));
    }

    vk::MemoryRequirements mem_requirements = m_context.get_device().getBufferMemoryRequirements(out_buffer);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, memory_properties);
    vk::MemoryAllocateFlagsInfo alloc_flags {};
    alloc_flags.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;
    alloc_info.pNext = &alloc_flags;

    try {
        out_memory = m_context.get_device().allocateMemory(alloc_info);
    } catch (const vk::SystemError& e) {
        m_context.get_device().destroyBuffer(out_buffer);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to allocate raw VkDeviceMemory: " + std::string(e.what()));
    }

    try {
        m_context.get_device().bindBufferMemory(out_buffer, out_memory, 0);
    } catch (const vk::SystemError& e) {
        m_context.get_device().freeMemory(out_memory);
        m_context.get_device().destroyBuffer(out_buffer);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to bind raw buffer memory: " + std::string(e.what()));
    }

    out_mapped_ptr = nullptr;
    if (host_visible) {
        try {
            out_mapped_ptr = m_context.get_device().mapMemory(out_memory, 0, VK_WHOLE_SIZE);
        } catch (const vk::SystemError& e) {
            m_context.get_device().freeMemory(out_memory);
            m_context.get_device().destroyBuffer(out_buffer);
            error_rethrow(
                Journal::Component::Core,
                Journal::Context::GraphicsBackend,
                std::source_location::current(),
                "Failed to map raw buffer memory: " + std::string(e.what()));
        }
    }

    m_buffer_allocation_sizes[static_cast<VkDeviceMemory>(out_memory)] = mem_requirements.size;
}

void BackendResourceManager::initialize_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to initialize null VKBuffer");
        return;
    }
    if (buffer->is_initialized()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKBuffer already initialized, skipping");
        return;
    }

    vk::Buffer vk_buffer;
    vk::DeviceMemory memory;
    void* mapped_ptr = nullptr;

    allocate_raw_buffer(
        buffer->get_size_bytes(),
        buffer->get_usage_flags(),
        vk::MemoryPropertyFlags(buffer->get_memory_properties()),
        buffer->is_host_visible(),
        vk_buffer, memory, mapped_ptr);

    auto& resources = buffer->get_buffer_resources();
    resources.buffer = vk_buffer;
    resources.memory = memory;
    resources.mapped_ptr = mapped_ptr;

    m_managed_buffers.push_back(buffer);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKBuffer initialized: {} bytes, modality: {}, VkBuffer: {:p}",
        buffer->get_size_bytes(),
        Kakshya::modality_to_string(buffer->get_modality()),
        (void*)buffer->get_buffer());
}

void BackendResourceManager::cleanup_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to cleanup null VKBuffer");
        return;
    }

    auto it = std::ranges::find(m_managed_buffers, buffer);
    if (it == m_managed_buffers.end()) {
        return;
    }

    auto& res = it->get()->get_buffer_resources();

    release_buffer_handles(res.buffer, res.memory, res.mapped_ptr != nullptr);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKBuffer cleaned up: {:p}", static_cast<void*>(res.buffer));

    res.mapped_ptr = nullptr;
    res.buffer = vk::Buffer {};
    res.memory = vk::DeviceMemory {};

    m_managed_buffers.erase(it);
}

void BackendResourceManager::retire_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer)
{
    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to retire null VKBuffer");
        return;
    }

    auto it = std::ranges::find(m_managed_buffers, buffer);
    if (it == m_managed_buffers.end()) {
        return;
    }

    auto& res = it->get()->get_buffer_resources();
    const vk::Buffer old_buffer = res.buffer;
    const vk::DeviceMemory old_memory = res.memory;
    const bool mapped = res.mapped_ptr != nullptr;

    res.mapped_ptr = nullptr;
    res.buffer = vk::Buffer {};
    res.memory = vk::DeviceMemory {};

    m_managed_buffers.erase(it);

    m_command_manager.retire_resource(
        [this, old_buffer, old_memory, mapped]() {
            release_buffer_handles(old_buffer, old_memory, mapped);
        },
        m_command_manager.submitted());
}

void BackendResourceManager::release_buffer_handles(vk::Buffer buffer, vk::DeviceMemory memory, bool mapped)
{
    if (mapped) {
        m_context.get_device().unmapMemory(memory);
    }

    if (buffer) {
        m_context.get_device().destroyBuffer(buffer);
    }

    if (memory) {
        m_buffer_allocation_sizes.erase(static_cast<VkDeviceMemory>(memory));
        m_context.get_device().freeMemory(memory);
    }
}

void BackendResourceManager::flush_pending_buffer_operations()
{
    for (auto& buffer_wrapper : m_managed_buffers) {
        auto& resources = buffer_wrapper->get_buffer_resources();
        auto dirty_ranges = buffer_wrapper->get_and_clear_dirty_ranges();
        if (!dirty_ranges.empty()) {
            for (auto [offset, size] : dirty_ranges) {
                auto range = mapped_memory_range(resources.memory, offset, size);
                if (auto result = m_context.get_device().flushMappedMemoryRanges(1, &range); result != vk::Result::eSuccess) {
                    MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                        "Failed to flush mapped memory range: {}", vk::to_string(result));
                }
            }
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Flushed {} dirty ranges for buffer {:p}", dirty_ranges.size(),
                (void*)buffer_wrapper->get_buffer());
        }

        auto invalid_ranges = buffer_wrapper->get_and_clear_invalid_ranges();
        if (!invalid_ranges.empty()) {
            for (auto [offset, size] : invalid_ranges) {
                auto range = mapped_memory_range(buffer_wrapper->get_buffer_resources().memory, offset, size);
                if (auto result = m_context.get_device().invalidateMappedMemoryRanges(1, &range); result != vk::Result::eSuccess) {
                    MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                        "Failed to invalidate mapped memory range: {}", vk::to_string(result));
                }
            }
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Invalidated {} ranges for buffer {:p}", invalid_ranges.size(),
                (void*)buffer_wrapper->get_buffer());
        }
    }
}

vk::DeviceAddress BackendResourceManager::get_buffer_device_address(
    const std::shared_ptr<Buffers::VKBuffer>& buffer) const
{
    if (!buffer || !buffer->is_initialized()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "get_buffer_device_address: buffer not initialized");
        return 0;
    }

    vk::BufferDeviceAddressInfo info {};
    info.buffer = buffer->get_buffer();
    // return static_cast<uint64_t>(m_context.get_device().getBufferAddress(info));
    return m_context.get_device().getBufferAddress(info);
}

void BackendResourceManager::initialize_image(const std::shared_ptr<VKImage>& image)
{
    if (!image) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Attempted to initialize null VKImage");
        return;
    }

    if (image->is_initialized()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKImage already initialized, skipping");
        return;
    }

    // ========================================================================
    // Step 1: Create VkImage
    // ========================================================================

    vk::ImageCreateInfo image_info {};

    switch (image->get_type()) {
    case VKImage::Type::TYPE_1D:
        image_info.imageType = vk::ImageType::e1D;
        break;
    case VKImage::Type::TYPE_2D:
    case VKImage::Type::TYPE_CUBE:
        image_info.imageType = vk::ImageType::e2D;
        break;
    case VKImage::Type::TYPE_3D:
        image_info.imageType = vk::ImageType::e3D;
        break;
    }

    image_info.extent.width = image->get_width();
    image_info.extent.height = image->get_height();
    image_info.extent.depth = image->get_depth();
    image_info.mipLevels = image->get_mip_levels();
    image_info.arrayLayers = image->get_array_layers();
    image_info.format = image->get_format();
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.usage = image->get_usage_flags();

    auto format_properties = vk::StructureChain {
        vk::FormatProperties2 {},
        vk::FormatProperties3 {}
    };
    m_context.get_physical_device().getFormatProperties2(
        image->get_format(), &format_properties.get<vk::FormatProperties2>());

    const bool host_transfer = static_cast<bool>(
        format_properties.get<vk::FormatProperties3>().optimalTilingFeatures
        & vk::FormatFeatureFlagBits2::eHostImageTransfer);

    if (host_transfer) {
        image_info.usage |= vk::ImageUsageFlagBits::eHostTransfer;
    }

    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.samples = vk::SampleCountFlagBits::e1; // No MSAA for now
    image_info.flags = (image->get_type() == VKImage::Type::TYPE_CUBE)
        ? vk::ImageCreateFlagBits::eCubeCompatible
        : vk::ImageCreateFlags {};

    vk::Image vk_image;
    try {
        vk_image = m_context.get_device().createImage(image_info);
    } catch (const vk::SystemError& e) {
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create VkImage: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 2: Allocate memory
    // ========================================================================

    vk::MemoryRequirements mem_requirements;
    mem_requirements = m_context.get_device().getImageMemoryRequirements(vk_image);

    vk::MemoryAllocateInfo alloc_info {};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        mem_requirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::DeviceMemory memory;
    try {
        memory = m_context.get_device().allocateMemory(alloc_info);
    } catch (const vk::SystemError& e) {
        m_context.get_device().destroyImage(vk_image);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to allocate VkDeviceMemory for image: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 3: Bind memory to image
    // ========================================================================

    try {
        m_context.get_device().bindImageMemory(vk_image, memory, 0);
    } catch (const vk::SystemError& e) {
        m_context.get_device().freeMemory(memory);
        m_context.get_device().destroyImage(vk_image);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to bind memory to VkImage: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 4: Create image view
    // ========================================================================

    vk::ImageViewCreateInfo view_info {};

    switch (image->get_type()) {
    case VKImage::Type::TYPE_1D:
        view_info.viewType = (image->get_array_layers() > 1)
            ? vk::ImageViewType::e1DArray
            : vk::ImageViewType::e1D;
        break;
    case VKImage::Type::TYPE_2D:
        view_info.viewType = (image->get_array_layers() > 1)
            ? vk::ImageViewType::e2DArray
            : vk::ImageViewType::e2D;
        break;
    case VKImage::Type::TYPE_3D:
        view_info.viewType = vk::ImageViewType::e3D;
        break;
    case VKImage::Type::TYPE_CUBE:
        view_info.viewType = vk::ImageViewType::eCube;
        break;
    }

    view_info.image = vk_image;
    view_info.format = image->get_format();
    view_info.subresourceRange.aspectMask = image->get_aspect_flags();
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = image->get_mip_levels();
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = image->get_array_layers();

    view_info.components.r = vk::ComponentSwizzle::eIdentity;
    view_info.components.g = vk::ComponentSwizzle::eIdentity;
    view_info.components.b = vk::ComponentSwizzle::eIdentity;
    view_info.components.a = vk::ComponentSwizzle::eIdentity;

    vk::ImageView image_view;
    try {
        image_view = m_context.get_device().createImageView(view_info);
    } catch (const vk::SystemError& e) {
        m_context.get_device().freeMemory(memory);
        m_context.get_device().destroyImage(vk_image);
        error_rethrow(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Failed to create VkImageView: " + std::string(e.what()));
    }

    // ========================================================================
    // Step 5: Store handles in VKImage
    // ========================================================================

    VKImageResources resources {};
    resources.image = vk_image;
    resources.image_view = image_view;
    resources.memory = memory;
    resources.sampler = nullptr;

    image->set_image_resources(resources);
    image->set_current_layout(vk::ImageLayout::eUndefined);
    image->set_host_transfer(host_transfer);

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKImage initialized: {}x{}x{}, format: {}, {} mips, {} layers",
        image->get_width(), image->get_height(), image->get_depth(),
        vk::to_string(image->get_format()),
        image->get_mip_levels(), image->get_array_layers());
}

void BackendResourceManager::cleanup_image(const std::shared_ptr<VKImage>& image)
{
    if (!image || !image->is_initialized()) {
        return;
    }

    const auto& resources = image->get_image_resources();

    if (resources.image_view) {
        m_context.get_device().destroyImageView(resources.image_view);
    }

    if (resources.image) {
        m_context.get_device().destroyImage(resources.image);
    }

    if (resources.memory) {
        m_context.get_device().freeMemory(resources.memory);
    }

    image->set_image_resources(VKImageResources {});
    image->set_host_transfer(false);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "VKImage cleaned up");
}

void BackendResourceManager::load_host_copy_layouts()
{
    if (m_host_copy_layouts_ready) {
        return;
    }

    vk::PhysicalDeviceHostImageCopyProperties host_properties {};
    vk::PhysicalDeviceProperties2 properties {};
    properties.pNext = &host_properties;

    m_context.get_physical_device().getProperties2(&properties);

    m_host_copy_src_layouts.resize(host_properties.copySrcLayoutCount);
    m_host_copy_dst_layouts.resize(host_properties.copyDstLayoutCount);

    host_properties.pCopySrcLayouts = m_host_copy_src_layouts.data();
    host_properties.pCopyDstLayouts = m_host_copy_dst_layouts.data();

    m_context.get_physical_device().getProperties2(&properties);

    m_host_copy_src_layouts.resize(host_properties.copySrcLayoutCount);
    m_host_copy_dst_layouts.resize(host_properties.copyDstLayoutCount);

    m_host_copy_layouts_ready = true;
}

bool BackendResourceManager::host_copy_src_layout(vk::ImageLayout layout) const
{
    return std::ranges::find(m_host_copy_src_layouts, layout)
        != m_host_copy_src_layouts.end();
}

bool BackendResourceManager::host_copy_dst_layout(vk::ImageLayout layout) const
{
    return std::ranges::find(m_host_copy_dst_layouts, layout)
        != m_host_copy_dst_layouts.end();
}

void BackendResourceManager::transition_host_image(
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::ImageSubresourceRange range,
    vk::PipelineStageFlags2 restore_stage)
{
    if (old_layout == new_layout) {
        return;
    }

    const bool can_transition_on_host = (old_layout == vk::ImageLayout::eUndefined || host_copy_src_layout(old_layout))
        && host_copy_dst_layout(new_layout);

    if (can_transition_on_host) {
        vk::HostImageLayoutTransitionInfo transition {};
        transition.image = image;
        transition.oldLayout = old_layout;
        transition.newLayout = new_layout;
        transition.subresourceRange = range;

        const auto result = m_context.get_device().transitionImageLayout(1, &transition);

        if (result != vk::Result::eSuccess) {
            error<std::runtime_error>(
                Journal::Component::Core,
                Journal::Context::GraphicsBackend,
                std::source_location::current(),
                "Host image layout transition failed: {}",
                vk::to_string(result));
        }

        return;
    }

    vk::AccessFlags2 source_access {};
    vk::PipelineStageFlags2 source_stage = vk::PipelineStageFlagBits2::eNone;

    if (old_layout == vk::ImageLayout::eGeneral) {
        source_access = vk::AccessFlagBits2::eHostWrite;
        source_stage = vk::PipelineStageFlagBits2::eHost;
    } else if (old_layout != vk::ImageLayout::eUndefined) {
        source_access = vk::AccessFlagBits2::eMemoryRead
            | vk::AccessFlagBits2::eMemoryWrite;
        source_stage = vk::PipelineStageFlagBits2::eAllCommands;
    }

    const bool needs_host_access = new_layout == vk::ImageLayout::eGeneral;
    const vk::AccessFlags2 destination_access = needs_host_access
        ? vk::AccessFlagBits2::eHostRead | vk::AccessFlagBits2::eHostWrite
        : vk::AccessFlags2(vk::AccessFlagBits2::eMemoryRead);
    const vk::PipelineStageFlags2 destination_stage = needs_host_access
        ? vk::PipelineStageFlags2(vk::PipelineStageFlagBits2::eHost)
        : restore_stage;

    execute_immediate_commands([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 barrier {};
        barrier.image = image;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.subresourceRange = range;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask = source_access;
        barrier.dstAccessMask = destination_access;

        record_barrier(cmd, barrier, source_stage, destination_stage);
    });
}

void BackendResourceManager::upload_image_host(
    const std::shared_ptr<VKImage>& image,
    uint32_t base_layer,
    uint32_t layer_count,
    const void* data,
    size_t size)
{
    const bool valid_image = image
        && image->is_initialized()
        && image->supports_host_transfer()
        && image->get_aspect_flags() == vk::ImageAspectFlagBits::eColor;

    const bool valid_layers = valid_image
        && layer_count > 0
        && base_layer < image->get_array_layers()
        && layer_count <= image->get_array_layers() - base_layer;

    if (!valid_layers || !data) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Host image upload requires an initialized host-transfer color image and valid layers");
    }

    const size_t layer_bytes = image->get_base_level_size_bytes();

    if (size != layer_bytes * layer_count) {
        error<std::invalid_argument>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Host image upload has {} bytes; {} required",
            size,
            layer_bytes * layer_count);
    }

    load_host_copy_layouts();

    if (m_command_manager.has_deferred_commands()) {
        flush_deferred_commands();
    }

    m_command_manager.wait(m_command_manager.submitted());

    const vk::ImageSubresourceRange all {
        image->get_aspect_flags(),
        0,
        image->get_mip_levels(),
        0,
        image->get_array_layers()
    };

    const vk::ImageSubresourceRange target {
        image->get_aspect_flags(),
        0,
        1,
        base_layer,
        layer_count
    };

    const auto old_layout = image->get_current_layout();
    const bool preserve_untouched = old_layout == vk::ImageLayout::eShaderReadOnlyOptimal;
    const auto transition_range = preserve_untouched ? target : all;

    transition_host_image(
        image->get_image(),
        preserve_untouched ? vk::ImageLayout::eUndefined : old_layout,
        vk::ImageLayout::eGeneral,
        transition_range);

    vk::MemoryToImageCopy region {};
    region.pHostPointer = data;
    region.imageSubresource = { image->get_aspect_flags(), 0, base_layer, layer_count };
    region.imageExtent = vk::Extent3D { image->get_width(), image->get_height(), image->get_depth() };

    vk::CopyMemoryToImageInfo copy {};
    copy.dstImage = image->get_image();
    copy.dstImageLayout = vk::ImageLayout::eGeneral;
    copy.regionCount = 1;
    copy.pRegions = &region;

    const auto result = m_context.get_device().copyMemoryToImage(&copy);

    if (result != vk::Result::eSuccess) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Host image upload failed: {}",
            vk::to_string(result));
    }

    transition_host_image(
        image->get_image(),
        vk::ImageLayout::eGeneral,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        transition_range,
        vk::PipelineStageFlagBits2::eFragmentShader
            | vk::PipelineStageFlagBits2::eComputeShader);

    image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);
}

void BackendResourceManager::download_image_host(
    const std::shared_ptr<VKImage>& image,
    void* data,
    size_t size,
    vk::ImageLayout restore_layout,
    vk::PipelineStageFlags2 restore_stage)
{
    const bool valid_image = image
        && image->is_initialized()
        && image->supports_host_transfer()
        && image->get_aspect_flags() == vk::ImageAspectFlagBits::eColor
        && image->get_current_layout() != vk::ImageLayout::eUndefined;

    if (!valid_image || !data) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Host image download requires an initialized host-transfer color image with defined layout");
    }

    const size_t layer_bytes = image->get_base_level_size_bytes();
    const uint32_t layer_count = size == layer_bytes ? 1U : image->get_array_layers();

    if (size != layer_bytes * layer_count) {
        error<std::invalid_argument>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Host image download has {} bytes; one layer needs {} and all layers need {}",
            size,
            layer_bytes,
            layer_bytes * image->get_array_layers());
    }

    load_host_copy_layouts();

    if (m_command_manager.has_deferred_commands()) {
        flush_deferred_commands();
    }

    m_command_manager.wait(m_command_manager.submitted());

    const auto old_layout = image->get_current_layout();

    const vk::ImageSubresourceRange range {
        image->get_aspect_flags(),
        0,
        1,
        0,
        layer_count
    };

    const auto copy_layout = host_copy_src_layout(old_layout)
        ? old_layout
        : vk::ImageLayout::eGeneral;

    execute_immediate_commands([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 barrier {};
        barrier.image = image->get_image();
        barrier.oldLayout = old_layout;
        barrier.newLayout = copy_layout;
        barrier.subresourceRange = range;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eHostRead;

        record_barrier(cmd, barrier,
            vk::PipelineStageFlagBits2::eAllCommands,
            vk::PipelineStageFlagBits2::eHost);
    });

    vk::ImageToMemoryCopy region {};
    region.pHostPointer = data;
    region.imageSubresource = { image->get_aspect_flags(), 0, 0, layer_count };
    region.imageExtent = vk::Extent3D { image->get_width(), image->get_height(), image->get_depth() };

    vk::CopyImageToMemoryInfo copy {};
    copy.srcImage = image->get_image();
    copy.srcImageLayout = copy_layout;
    copy.regionCount = 1;
    copy.pRegions = &region;

    const auto result = m_context.get_device().copyImageToMemory(&copy);

    if (result != vk::Result::eSuccess) {
        error<std::runtime_error>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Host image download failed: {}",
            vk::to_string(result));
    }

    transition_host_image(
        image->get_image(),
        copy_layout,
        restore_layout,
        range,
        restore_stage);

    image->set_current_layout(restore_layout);
}

void BackendResourceManager::transition_image_layout(
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    uint32_t mip_levels,
    uint32_t array_layers,
    vk::ImageAspectFlags aspect_flags)
{
    execute_immediate_commands([&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 barrier {};
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect_flags;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = array_layers;

        vk::PipelineStageFlags2 src_stage;
        vk::PipelineStageFlags2 dst_stage;

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = vk::AccessFlags2 {};
            barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
            src_stage = vk::PipelineStageFlagBits2::eNone;
            dst_stage = vk::PipelineStageFlagBits2::eTransfer;
        } else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            src_stage = vk::PipelineStageFlagBits2::eTransfer;
            dst_stage = vk::PipelineStageFlagBits2::eFragmentShader;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlags2 {};
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            src_stage = vk::PipelineStageFlagBits2::eNone;
            dst_stage = vk::PipelineStageFlagBits2::eFragmentShader;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eColorAttachmentOptimal) {
            barrier.srcAccessMask = vk::AccessFlags2 {};
            barrier.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
            src_stage = vk::PipelineStageFlagBits2::eNone;
            dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
            barrier.srcAccessMask = vk::AccessFlags2 {};
            barrier.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
            src_stage = vk::PipelineStageFlagBits2::eNone;
            dst_stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests;
        } else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eGeneral) {
            barrier.srcAccessMask = vk::AccessFlags2 {};
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite;
            src_stage = vk::PipelineStageFlagBits2::eNone;
            dst_stage = vk::PipelineStageFlagBits2::eComputeShader;
        } else if (old_layout == vk::ImageLayout::eGeneral && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite;
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            src_stage = vk::PipelineStageFlagBits2::eComputeShader;
            dst_stage = vk::PipelineStageFlagBits2::eComputeShader;
        } else if (old_layout == vk::ImageLayout::eShaderReadOnlyOptimal && new_layout == vk::ImageLayout::eGeneral) {
            barrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderWrite;
            src_stage = vk::PipelineStageFlagBits2::eComputeShader;
            dst_stage = vk::PipelineStageFlagBits2::eComputeShader;
        } else {
            barrier.srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
            barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
            src_stage = vk::PipelineStageFlagBits2::eAllCommands;
            dst_stage = vk::PipelineStageFlagBits2::eAllCommands;

            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Using generic image layout transition");
        }

        record_barrier(cmd, barrier, src_stage, dst_stage);
    });

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Image layout transitioned: {} -> {}",
        vk::to_string(old_layout), vk::to_string(new_layout));
}

void BackendResourceManager::upload_image_data(
    const std::shared_ptr<VKImage>& image,
    const void* data,
    size_t size,
    const std::shared_ptr<Buffers::VKBuffer>& staging,
    bool deferred)
{
    if (!image || !data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid parameters for upload_image_data");
        return;
    }

    if (!deferred) {
        upload_image_host(image, 0, image->get_array_layers(), data, size);

        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Uploaded {} bytes to image {}x{}",
            size, image->get_width(), image->get_height());

        return;
    }

    if (!staging) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Deferred image upload requires staging");
        return;
    }

    if (!image->is_initialized()
        || size != image->get_base_level_size_bytes() * image->get_array_layers()
        || staging->get_size_bytes() < size) {
        error<std::invalid_argument>(
            Journal::Component::Core,
            Journal::Context::GraphicsBackend,
            std::source_location::current(),
            "Deferred image upload requires initialized image storage and sufficient staging capacity");
    }

    void* mapped = staging->get_mapped_ptr();
    if (!mapped) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Deferred image upload staging buffer has no mapped pointer");
        return;
    }

    std::memcpy(mapped, data, size);
    staging->mark_dirty_range(0, size);

    auto& resources = staging->get_buffer_resources();
    auto range = mapped_memory_range(resources.memory, 0, VK_WHOLE_SIZE);

    if (auto result = m_context.get_device().flushMappedMemoryRanges(1, &range);
        result != vk::Result::eSuccess) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Deferred image upload staging flush failed: {}", vk::to_string(result));
    }

    auto record_command = [&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 barrier {};
        barrier.oldLayout = image->get_current_layout();
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->get_image();
        barrier.subresourceRange = {
            image->get_aspect_flags(), 0,
            image->get_mip_levels(), 0,
            image->get_array_layers()
        };
        barrier.srcAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;

        record_barrier(cmd, barrier, vk::PipelineStageFlagBits2::eFragmentShader, vk::PipelineStageFlagBits2::eTransfer);

        vk::BufferImageCopy region {};
        region.imageSubresource.aspectMask = image->get_aspect_flags();
        region.imageSubresource.layerCount = image->get_array_layers();
        region.imageOffset = vk::Offset3D { 0, 0, 0 };
        region.imageExtent = vk::Extent3D {
            image->get_width(),
            image->get_height(),
            image->get_depth()
        };

        cmd.copyBufferToImage(
            staging->get_buffer(),
            image->get_image(),
            vk::ImageLayout::eTransferDstOptimal,
            1, &region);

        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;

        record_barrier(cmd, barrier, vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eFragmentShader);
    };

    record_deferred_commands(record_command);

    image->set_current_layout(vk::ImageLayout::eShaderReadOnlyOptimal);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Deferred image upload: {} bytes to image {}x{}",
        size, image->get_width(), image->get_height());
}

void BackendResourceManager::download_image_data(
    const std::shared_ptr<VKImage>& image,
    void* data,
    size_t size,
    vk::ImageLayout restore_layout,
    vk::PipelineStageFlags2 restore_stage)
{
    if (!image || !data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "download_image_data: invalid parameters");
        return;
    }

    if (image->get_memory()) {
        download_image_host(image, data, size, restore_layout, restore_stage);
        return;
    }

    auto staging = std::make_shared<Buffers::VKBuffer>(
        size,
        Buffers::VKBuffer::Usage::STAGING,
        Kakshya::DataModality::IMAGE_COLOR);
    initialize_buffer(staging);

    auto record_command = [&](vk::CommandBuffer cmd) {
        vk::ImageMemoryBarrier2 barrier {};
        barrier.oldLayout = image->get_current_layout();
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->get_image();
        barrier.subresourceRange.aspectMask = image->get_aspect_flags();
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = image->get_mip_levels();
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = image->get_array_layers();
        barrier.srcAccessMask = vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;

        record_barrier(cmd, barrier, vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eComputeShader, vk::PipelineStageFlagBits2::eTransfer);

        vk::BufferImageCopy region {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = image->get_aspect_flags();
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = image->get_array_layers();
        region.imageOffset = vk::Offset3D { 0, 0, 0 };
        region.imageExtent = vk::Extent3D {
            image->get_width(),
            image->get_height(),
            image->get_depth()
        };

        cmd.copyImageToBuffer(
            image->get_image(),
            vk::ImageLayout::eTransferSrcOptimal,
            staging->get_buffer(),
            1, &region);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = restore_layout;
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits2::eMemoryRead;

        record_barrier(cmd, barrier, vk::PipelineStageFlagBits2::eTransfer, restore_stage);
    };

    auto device = m_context.get_device();

    vk::CommandBuffer cmd = m_command_manager.begin_single_time_commands();
    record_command(cmd);
    cmd.end();

    const vk::CommandBuffer commands[] = { cmd };
    const auto waits = m_command_manager.produced_waits(vk::PipelineStageFlagBits2::eAllCommands);
    const TimelinePoint point = m_command_manager.submit(m_context.get_graphics_queue(), commands, waits);
    if (!point) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "download_image_data: queue submit failed");
        m_command_manager.free_command_buffer(cmd);
        cleanup_buffer(staging);
        return;
    }

    image->set_current_layout(restore_layout);

    m_command_manager.wait(point);

    auto range = mapped_memory_range(staging->get_buffer_resources().memory, 0, VK_WHOLE_SIZE);
    if (auto result = device.invalidateMappedMemoryRanges(1, &range);
        result != vk::Result::eSuccess) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "download_image_data: invalidate failed: {}", vk::to_string(result));
    }

    void* mapped = staging->get_mapped_ptr();
    if (mapped) {
        std::memcpy(data, mapped, size);
    } else {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "download_image_data: staging buffer has no mapped pointer");
    }

    m_command_manager.free_command_buffer(cmd);

    cleanup_buffer(staging);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "download_image_data: {} bytes from image {}x{}",
        size, image->get_width(), image->get_height());
}

bool BackendResourceManager::upload_image_layer(
    const std::shared_ptr<VKImage>& image,
    uint32_t layer,
    const void* data,
    size_t size)
{
    if (!image || !data || layer >= image->get_array_layers()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "upload_image_layer: invalid image, data or layer");
        return false;
    }

    const size_t layer_bytes = image->get_base_level_size_bytes();
    if (size != layer_bytes) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "upload_image_layer: {} bytes given, layer needs {}", size, layer_bytes);
        return false;
    }

    upload_image_host(image, layer, 1, data, size);

    return true;
}

bool BackendResourceManager::supports_blit(
    vk::Format src_format, vk::Format dst_format, vk::Filter filter) const
{
    const auto physical = m_context.get_physical_device();
    const auto src_features = physical.getFormatProperties(src_format).optimalTilingFeatures;
    const auto dst_features = physical.getFormatProperties(dst_format).optimalTilingFeatures;

    if (!(src_features & vk::FormatFeatureFlagBits::eBlitSrc)
        || !(dst_features & vk::FormatFeatureFlagBits::eBlitDst)) {
        return false;
    }

    return filter != vk::Filter::eLinear
        || static_cast<bool>(src_features & vk::FormatFeatureFlagBits::eSampledImageFilterLinear);
}

bool BackendResourceManager::blit_image(
    const std::shared_ptr<VKImage>& src,
    const std::shared_ptr<VKImage>& dst,
    const vk::ImageBlit& region,
    vk::Filter filter,
    bool deferred)
{
    if (!src || !dst || src == dst) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "blit_image: source and destination must be two distinct initialised images");
        return false;
    }

    if (!supports_blit(src->get_format(), dst->get_format(), filter)) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "blit_image: formats {} to {} do not support a {} blit",
            vk::to_string(src->get_format()), vk::to_string(dst->get_format()), vk::to_string(filter));
        return false;
    }

    const auto settled_layout = [](const std::shared_ptr<VKImage>& image) {
        const auto layout = image->get_current_layout();
        const bool transient = layout == vk::ImageLayout::eUndefined
            || layout == vk::ImageLayout::eTransferSrcOptimal
            || layout == vk::ImageLayout::eTransferDstOptimal;
        return transient ? vk::ImageLayout::eShaderReadOnlyOptimal : layout;
    };
    const auto src_restore = settled_layout(src);
    const auto dst_restore = settled_layout(dst);

    auto record_command = [&](vk::CommandBuffer cmd) {
        const auto transition = [&cmd](
                                    const std::shared_ptr<VKImage>& image,
                                    vk::ImageLayout from,
                                    vk::ImageLayout to,
                                    vk::AccessFlags2 src_access,
                                    vk::AccessFlags2 dst_access,
                                    vk::PipelineStageFlags2 src_stage,
                                    vk::PipelineStageFlags2 dst_stage) {
            vk::ImageMemoryBarrier2 barrier {};
            barrier.oldLayout = from;
            barrier.newLayout = to;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image->get_image();
            barrier.subresourceRange = {
                image->get_aspect_flags(), 0,
                image->get_mip_levels(), 0,
                image->get_array_layers()
            };
            barrier.srcAccessMask = src_access;
            barrier.dstAccessMask = dst_access;
            record_barrier(cmd, barrier, src_stage, dst_stage);
        };

        constexpr auto any_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;

        transition(src, src->get_current_layout(), vk::ImageLayout::eTransferSrcOptimal,
            any_access, vk::AccessFlagBits2::eTransferRead,
            vk::PipelineStageFlagBits2::eAllCommands, vk::PipelineStageFlagBits2::eTransfer);
        transition(dst, dst->get_current_layout(), vk::ImageLayout::eTransferDstOptimal,
            any_access, vk::AccessFlagBits2::eTransferWrite,
            vk::PipelineStageFlagBits2::eAllCommands, vk::PipelineStageFlagBits2::eTransfer);

        cmd.blitImage(
            src->get_image(), vk::ImageLayout::eTransferSrcOptimal,
            dst->get_image(), vk::ImageLayout::eTransferDstOptimal,
            1, &region, filter);

        transition(src, vk::ImageLayout::eTransferSrcOptimal, src_restore,
            vk::AccessFlagBits2::eTransferRead, any_access,
            vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eAllCommands);
        transition(dst, vk::ImageLayout::eTransferDstOptimal, dst_restore,
            vk::AccessFlagBits2::eTransferWrite, any_access,
            vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eAllCommands);
    };

    if (deferred) {
        record_deferred_commands(record_command);
    } else {
        execute_immediate_commands(record_command);
    }

    src->set_current_layout(src_restore);
    dst->set_current_layout(dst_restore);

    return true;
}

vk::Sampler BackendResourceManager::create_sampler(
    vk::Filter filter,
    vk::SamplerAddressMode address_mode,
    float max_anisotropy)
{
    size_t hash = 0;
    auto hash_combine = [](size_t& seed, size_t value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hash_combine(hash, static_cast<size_t>(filter));
    hash_combine(hash, static_cast<size_t>(address_mode));
    hash_combine(hash, std::hash<float> {}(max_anisotropy));

    auto it = m_sampler_cache.find(hash);
    if (it != m_sampler_cache.end()) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Reusing cached sampler (hash: 0x{:X})", hash);
        return it->second;
    }

    vk::SamplerCreateInfo sampler_info;
    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;
    sampler_info.mipLodBias = 0.0F;
    sampler_info.anisotropyEnable = max_anisotropy > 0.0F;
    sampler_info.maxAnisotropy = max_anisotropy;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.minLod = 0.0F;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;
    sampler_info.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    vk::Sampler sampler;
    try {
        sampler = m_context.get_device().createSampler(sampler_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create sampler: {}", e.what());
        return nullptr;
    }

    m_sampler_cache[hash] = sampler;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Created sampler (filter: {}, address: {}, anisotropy: {}, hash: 0x{:X})",
        vk::to_string(filter), vk::to_string(address_mode), max_anisotropy, hash);

    return sampler;
}

void BackendResourceManager::destroy_sampler(vk::Sampler sampler)
{
    if (!sampler) {
        return;
    }

    for (auto it = m_sampler_cache.begin(); it != m_sampler_cache.end(); ++it) {
        if (it->second == sampler) {
            m_sampler_cache.erase(it);
            break;
        }
    }

    m_context.get_device().destroySampler(sampler);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Destroyed sampler");
}

uint32_t BackendResourceManager::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const
{
    vk::PhysicalDeviceMemoryProperties mem_properties;
    mem_properties = m_context.get_physical_device().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    error<std::runtime_error>(
        Journal::Component::Core,
        Journal::Context::GraphicsBackend,
        std::source_location::current(),
        "Failed to find suitable memory type");

    return 0;
}

void BackendResourceManager::execute_immediate_commands(const std::function<void(vk::CommandBuffer)>& recorder)
{
    vk::CommandBuffer cmd = m_command_manager.begin_single_time_commands();
    recorder(cmd);
    m_command_manager.end_single_time_commands(cmd, m_context.get_graphics_queue());
}

void BackendResourceManager::record_deferred_commands(const std::function<void(vk::CommandBuffer)>& recorder)
{
    m_command_manager.record_deferred_commands(recorder);
}

TimelinePoint BackendResourceManager::flush_deferred_commands()
{
    return m_command_manager.flush_deferred(m_context.get_graphics_queue());
}

void BackendResourceManager::cleanup()
{
    for (auto& [hash, sampler] : m_sampler_cache) {
        if (sampler) {
            m_context.get_device().destroySampler(sampler);
        }
    }
    m_sampler_cache.clear();

    const auto device = m_context.get_device();
    auto release_slots = [this, &device](Buffers::VKBuffer& owner) {
        auto& slots = owner.get_buffer_resources().back_buffers;
        for (auto& slot : slots) {
            if (slot.mapped_ptr && slot.memory) {
                device.unmapMemory(slot.memory);
            }
            if (slot.buffer) {
                device.destroyBuffer(slot.buffer);
            }
            if (slot.memory) {
                m_buffer_allocation_sizes.erase(static_cast<VkDeviceMemory>(slot.memory));
                device.freeMemory(slot.memory);
            }
        }
        slots.clear();
    };

    auto buffers = m_managed_buffers;
    for (const auto& buffer : buffers) {
        if (!buffer) {
            continue;
        }
        release_slots(*buffer);
        if (buffer->is_initialized()) {
            cleanup_buffer(buffer);
        }
    }
    m_managed_buffers.clear();
    m_buffer_allocation_sizes.clear();
    m_host_copy_layouts_ready = false;
    m_host_copy_src_layouts.clear();
    m_host_copy_dst_layouts.clear();
}

size_t BackendResourceManager::compute_sampler_hash(vk::Filter filter, vk::SamplerAddressMode address_mode, float max_anisotropy) const
{
    size_t hash = 0;
    auto hash_combine = [](size_t& seed, size_t value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    hash_combine(hash, static_cast<size_t>(filter));
    hash_combine(hash, static_cast<size_t>(address_mode));
    hash_combine(hash, std::hash<float> {}(max_anisotropy));

    return hash;
}

}
