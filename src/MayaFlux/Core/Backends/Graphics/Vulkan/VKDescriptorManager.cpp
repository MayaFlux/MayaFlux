#include "VKDescriptorManager.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Core {

// ============================================================================
// Lifecycle
// ============================================================================

VKDescriptorManager::~VKDescriptorManager()
{
    if (!m_pools.empty() || !m_layouts.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKDescriptorManager destroyed without cleanup() - potential leak");
    }
}

VKDescriptorManager::VKDescriptorManager(VKDescriptorManager&& other) noexcept
    : m_device(other.m_device)
    , m_pools(std::move(other.m_pools))
    , m_current_pool_index(other.m_current_pool_index)
    , m_pool_size(other.m_pool_size)
    , m_allocated_count(other.m_allocated_count)
    , m_pool_capacity(other.m_pool_capacity)
    , m_layouts(std::move(other.m_layouts))
    , m_layout_cache(std::move(other.m_layout_cache))
{
    other.m_device = nullptr;
    other.m_pools.clear();
    other.m_layouts.clear();
}

VKDescriptorManager& VKDescriptorManager::operator=(VKDescriptorManager&& other) noexcept
{
    if (this != &other) {
        if (!m_pools.empty() || !m_layouts.empty()) {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "VKDescriptorManager move-assigned without cleanup() - potential leak");
        }

        m_device = other.m_device;
        m_pools = std::move(other.m_pools);
        m_current_pool_index = other.m_current_pool_index;
        m_pool_size = other.m_pool_size;
        m_allocated_count = other.m_allocated_count;
        m_pool_capacity = other.m_pool_capacity;
        m_layouts = std::move(other.m_layouts);
        m_layout_cache = std::move(other.m_layout_cache);

        other.m_device = nullptr;
        other.m_pools.clear();
        other.m_layouts.clear();
    }
    return *this;
}

bool VKDescriptorManager::initialize(vk::Device device, uint32_t initial_pool_size)
{
    if (!device) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot initialize descriptor manager with null device");
        return false;
    }

    m_device = device;
    m_pool_size = initial_pool_size;

    auto pool = create_pool(device, m_pool_size);
    if (!pool) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create initial descriptor pool");
        return false;
    }

    m_pools.push_back(pool);
    m_pool_capacity = m_pool_size;
    m_current_pool_index = 0;
    m_allocated_count = 0;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Descriptor manager initialized (pool size: {} sets)", m_pool_size);

    return true;
}

void VKDescriptorManager::cleanup(vk::Device device)
{
    if (!device) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "cleanup() called with null device");
        return;
    }

    for (auto layout : m_layouts) {
        if (layout) {
            device.destroyDescriptorSetLayout(layout);
        }
    }
    m_layouts.clear();
    m_layout_cache.clear();

    for (auto pool : m_pools) {
        if (pool) {
            device.destroyDescriptorPool(pool);
        }
    }
    m_pools.clear();

    m_current_pool_index = 0;
    m_allocated_count = 0;
    m_pool_capacity = 0;
    m_device = nullptr;

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Descriptor manager cleaned up");
}

// ============================================================================
// Descriptor Set Layout
// ============================================================================

vk::DescriptorSetLayout VKDescriptorManager::create_layout(
    vk::Device device,
    const DescriptorSetLayoutConfig& config)
{
    if (config.bindings.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Creating descriptor set layout with no bindings");
    }

    size_t config_hash = hash_layout_config(config);
    auto it = m_layout_cache.find(config_hash);
    if (it != m_layout_cache.end()) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Reusing cached descriptor set layout (hash: 0x{:X})", config_hash);
        return m_layouts[it->second];
    }

    std::vector<vk::DescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(config.bindings.size());

    for (const auto& binding : config.bindings) {
        vk::DescriptorSetLayoutBinding vk_binding;
        vk_binding.binding = binding.binding;
        vk_binding.descriptorType = binding.type;
        vk_binding.descriptorCount = binding.count;
        vk_binding.stageFlags = binding.stage_flags;
        vk_binding.pImmutableSamplers = nullptr;

        vk_bindings.push_back(vk_binding);
    }

    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.bindingCount = static_cast<uint32_t>(vk_bindings.size());
    layout_info.pBindings = vk_bindings.data();

    vk::DescriptorSetLayout layout;
    try {
        layout = device.createDescriptorSetLayout(layout_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create descriptor set layout: {}", e.what());
        return nullptr;
    }

    size_t layout_index = m_layouts.size();
    m_layouts.push_back(layout);
    m_layout_cache[config_hash] = layout_index;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Created descriptor set layout ({} bindings, hash: 0x{:X})",
        config.bindings.size(), config_hash);

    return layout;
}

size_t VKDescriptorManager::hash_layout_config(const DescriptorSetLayoutConfig& config) const
{
    size_t hash = 0;

    auto hash_combine = [](size_t& seed, size_t value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    for (const auto& binding : config.bindings) {
        hash_combine(hash, binding.binding);
        hash_combine(hash, static_cast<size_t>(binding.type));
        hash_combine(hash, binding.count);
        hash_combine(hash, static_cast<uint32_t>(binding.stage_flags));
    }

    return hash;
}

// ============================================================================
// Descriptor Pool Management
// ============================================================================

vk::DescriptorPool VKDescriptorManager::create_pool(vk::Device device, uint32_t max_sets)
{
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eStorageBuffer, max_sets * 4 },

        { vk::DescriptorType::eUniformBuffer, max_sets * 2 },

        { vk::DescriptorType::eStorageImage, max_sets * 2 },

        { vk::DescriptorType::eCombinedImageSampler, max_sets * 2 },
    };

    vk::DescriptorPoolCreateInfo pool_info;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = max_sets;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    vk::DescriptorPool pool;
    try {
        pool = device.createDescriptorPool(pool_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create descriptor pool: {}", e.what());
        return nullptr;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Created descriptor pool (max sets: {})", max_sets);

    return pool;
}

bool VKDescriptorManager::grow_pools(vk::Device device)
{
    auto new_pool = create_pool(device, m_pool_size);
    if (!new_pool) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to grow descriptor pool");
        return false;
    }

    m_pools.push_back(new_pool);
    m_pool_capacity += m_pool_size;
    m_current_pool_index = m_pools.size() - 1;

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Grew descriptor pools (new capacity: {} sets)", m_pool_capacity);

    return true;
}

// ============================================================================
// Descriptor Set Allocation
// ============================================================================

vk::DescriptorSet VKDescriptorManager::allocate_set(
    vk::Device device,
    vk::DescriptorSetLayout layout)
{
    if (!layout) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot allocate descriptor set with null layout");
        return nullptr;
    }

    if (m_pools.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "No descriptor pools available - call initialize() first");
        return nullptr;
    }

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = m_pools[m_current_pool_index];
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    vk::DescriptorSet set;
    try {
        auto sets = device.allocateDescriptorSets(alloc_info);
        set = sets[0];
        m_allocated_count++;
    } catch (const vk::OutOfPoolMemoryError&) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Descriptor pool {} exhausted, growing...", m_current_pool_index);

        if (!grow_pools(device)) {
            return nullptr;
        }

        alloc_info.descriptorPool = m_pools[m_current_pool_index];
        try {
            auto sets = device.allocateDescriptorSets(alloc_info);
            set = sets[0];
            m_allocated_count++;
        } catch (const vk::SystemError& e) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to allocate descriptor set after pool growth: {}", e.what());
            return nullptr;
        }
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to allocate descriptor set: {}", e.what());
        return nullptr;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Allocated descriptor set (total: {}/{})", m_allocated_count, m_pool_capacity);

    return set;
}

// ============================================================================
// Descriptor Set Updates
// ============================================================================

void VKDescriptorManager::update_buffer(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t binding,
    vk::Buffer buffer,
    vk::DeviceSize offset,
    vk::DeviceSize range)
{
    if (!set) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot update null descriptor set");
        return;
    }

    if (!buffer) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null buffer to descriptor set");
        return;
    }

    vk::DescriptorBufferInfo buffer_info;
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range = range;

    vk::WriteDescriptorSet write;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    // Note: type is inferred from layout, but we assume storage buffer
    // For flexibility, could add type parameter if needed
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.pBufferInfo = &buffer_info;

    device.updateDescriptorSets(1, &write, 0, nullptr);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Updated descriptor set binding {} with buffer (offset: {}, range: {})",
        binding, offset, range);
}

void VKDescriptorManager::update_image(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t binding,
    vk::ImageView image_view,
    vk::Sampler sampler,
    vk::ImageLayout layout)
{
    if (!set) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot update null descriptor set");
        return;
    }

    if (!image_view) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null image view to descriptor set");
        return;
    }

    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.sampler = sampler;
    image_info.imageLayout = layout;

    vk::WriteDescriptorSet write;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = sampler ? vk::DescriptorType::eCombinedImageSampler : vk::DescriptorType::eStorageImage;
    write.pImageInfo = &image_info;

    device.updateDescriptorSets(1, &write, 0, nullptr);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Updated descriptor set binding {} with image (layout: {})",
        binding, vk::to_string(layout));
}

void VKDescriptorManager::update_sampler(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t binding,
    vk::Sampler sampler)
{
    if (!set) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot update null descriptor set");
        return;
    }

    if (!sampler) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null sampler to descriptor set");
        return;
    }

    vk::DescriptorImageInfo image_info;
    image_info.sampler = sampler;
    image_info.imageView = nullptr;
    image_info.imageLayout = vk::ImageLayout::eUndefined;

    vk::WriteDescriptorSet write;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eSampler;
    write.pImageInfo = &image_info;

    device.updateDescriptorSets(1, &write, 0, nullptr);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Updated descriptor set binding {} with sampler", binding);
}

void VKDescriptorManager::update_combined_image_sampler(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t binding,
    vk::ImageView image_view,
    vk::Sampler sampler,
    vk::ImageLayout layout)
{
    if (!set) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot update null descriptor set");
        return;
    }

    if (!image_view || !sampler) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null image view or sampler");
        return;
    }

    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.sampler = sampler;
    image_info.imageLayout = layout;

    vk::WriteDescriptorSet write;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &image_info;

    device.updateDescriptorSets(1, &write, 0, nullptr);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Updated descriptor set binding {} with combined image sampler", binding);
}

void VKDescriptorManager::update_input_attachment(
    vk::Device device,
    vk::DescriptorSet set,
    uint32_t binding,
    vk::ImageView image_view,
    vk::ImageLayout layout)
{
    if (!set) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot update null descriptor set");
        return;
    }

    if (!image_view) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot bind null image view to input attachment");
        return;
    }

    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.sampler = nullptr;
    image_info.imageLayout = layout;

    vk::WriteDescriptorSet write;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eInputAttachment;
    write.pImageInfo = &image_info;

    device.updateDescriptorSets(1, &write, 0, nullptr);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Updated descriptor set binding {} with input attachment", binding);
}

void VKDescriptorManager::copy_descriptor_set(
    vk::Device device,
    vk::DescriptorSet src,
    vk::DescriptorSet dst,
    uint32_t copy_count)
{
    if (!src || !dst) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot copy null descriptor sets");
        return;
    }

    vk::CopyDescriptorSet copy;
    copy.srcSet = src;
    copy.srcBinding = 0;
    copy.srcArrayElement = 0;
    copy.dstSet = dst;
    copy.dstBinding = 0;
    copy.dstArrayElement = 0;
    copy.descriptorCount = copy_count;

    device.updateDescriptorSets(0, nullptr, 1, &copy);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Copied descriptor set ({} descriptors)", copy_count);
}

std::vector<vk::DescriptorSet> VKDescriptorManager::allocate_sets(
    vk::Device device,
    const std::vector<vk::DescriptorSetLayout>& layouts)
{
    if (layouts.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Allocating zero descriptor sets");
        return {};
    }

    if (m_pools.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "No descriptor pools available - call initialize() first");
        return {};
    }

    vk::DescriptorSetAllocateInfo alloc_info;
    alloc_info.descriptorPool = m_pools[m_current_pool_index];
    alloc_info.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    std::vector<vk::DescriptorSet> sets;
    try {
        sets = device.allocateDescriptorSets(alloc_info);
        m_allocated_count += static_cast<uint32_t>(layouts.size());
    } catch (const vk::OutOfPoolMemoryError&) {
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Descriptor pool {} exhausted, growing...", m_current_pool_index);

        if (!grow_pools(device)) {
            return {};
        }

        alloc_info.descriptorPool = m_pools[m_current_pool_index];
        try {
            sets = device.allocateDescriptorSets(alloc_info);
            m_allocated_count += static_cast<uint32_t>(layouts.size());
        } catch (const vk::SystemError& e) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to allocate descriptor sets after pool growth: {}", e.what());
            return {};
        }
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to allocate descriptor sets: {}", e.what());
        return {};
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Allocated {} descriptor sets (total: {}/{})",
        layouts.size(), m_allocated_count, m_pool_capacity);

    return sets;
}

void VKDescriptorManager::batch_update(
    vk::Device device,
    const std::vector<vk::WriteDescriptorSet>& writes)
{
    if (writes.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Batch update called with no writes");
        return;
    }

    device.updateDescriptorSets(
        static_cast<uint32_t>(writes.size()),
        writes.data(),
        0, nullptr);

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Batch updated {} descriptor bindings", writes.size());
}

void VKDescriptorManager::reset_pools(vk::Device device)
{
    if (m_pools.empty()) {
        return;
    }

    for (auto pool : m_pools) {
        if (pool) {
            try {
                device.resetDescriptorPool(pool);
            } catch (const vk::SystemError& e) {
                MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                    "Failed to reset descriptor pool: {}", e.what());
            }
        }
    }

    m_allocated_count = 0;
    m_current_pool_index = 0;

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Reset all descriptor pools");
}

// ============================================================================
// DescriptorUpdateBatch Implementation
// ============================================================================

DescriptorUpdateBatch::DescriptorUpdateBatch(vk::Device device, vk::DescriptorSet set)
    : m_device(device)
    , m_set(set)
{
}

DescriptorUpdateBatch& DescriptorUpdateBatch::buffer(
    uint32_t binding,
    vk::Buffer buffer,
    vk::DeviceSize offset,
    vk::DeviceSize range)
{
    vk::DescriptorBufferInfo buffer_info;
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range = range;
    m_buffer_infos.push_back(buffer_info);

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.pBufferInfo = &m_buffer_infos.back();
    m_writes.push_back(write);

    return *this;
}

DescriptorUpdateBatch& DescriptorUpdateBatch::storage_image(
    uint32_t binding,
    vk::ImageView image_view,
    vk::ImageLayout layout)
{
    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.sampler = nullptr;
    image_info.imageLayout = layout;
    m_image_infos.push_back(image_info);

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eStorageImage;
    write.pImageInfo = &m_image_infos.back();
    m_writes.push_back(write);

    return *this;
}

DescriptorUpdateBatch& DescriptorUpdateBatch::combined_image_sampler(
    uint32_t binding,
    vk::ImageView image_view,
    vk::Sampler sampler,
    vk::ImageLayout layout)
{
    vk::DescriptorImageInfo image_info;
    image_info.imageView = image_view;
    image_info.sampler = sampler;
    image_info.imageLayout = layout;
    m_image_infos.push_back(image_info);

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    write.pImageInfo = &m_image_infos.back();
    m_writes.push_back(write);

    return *this;
}

DescriptorUpdateBatch& DescriptorUpdateBatch::sampler(
    uint32_t binding,
    vk::Sampler sampler)
{
    vk::DescriptorImageInfo image_info;
    image_info.sampler = sampler;
    image_info.imageView = nullptr;
    image_info.imageLayout = vk::ImageLayout::eUndefined;
    m_image_infos.push_back(image_info);

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = vk::DescriptorType::eSampler;
    write.pImageInfo = &m_image_infos.back();
    m_writes.push_back(write);

    return *this;
}

void DescriptorUpdateBatch::submit()
{
    if (m_writes.empty()) {
        return;
    }

    m_device.updateDescriptorSets(
        static_cast<uint32_t>(m_writes.size()),
        m_writes.data(),
        0, nullptr);
}

} // namespace MayaFlux::Core
