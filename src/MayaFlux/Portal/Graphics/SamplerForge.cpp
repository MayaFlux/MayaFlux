#include "SamplerForge.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKContext.hpp"
#include "MayaFlux/Core/Backends/Graphics/Vulkan/VulkanBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "TextureLoom.hpp"

namespace MayaFlux::Portal::Graphics {

bool SamplerForge::s_initialized = false;

bool SamplerForge::initialize(const std::shared_ptr<Core::VulkanBackend>& backend)
{
    if (s_initialized) {
        MF_WARN(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "SamplerForge already initialized (static flag)");
        return true;
    }

    if (!backend) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Cannot initialize SamplerForge with null backend");
        return false;
    }

    if (m_backend) {
        MF_WARN(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "SamplerForge already initialized");
        return true;
    }

    m_backend = backend;

    s_initialized = true;

    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "SamplerForge initialized");

    return true;
}

void SamplerForge::shutdown()
{
    if (!s_initialized) {
        return;
    }

    if (!m_backend) {
        return;
    }

    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "Shutting down SamplerForge...");

    auto device = m_backend->get_context().get_device();
    for (auto& [hash, sampler] : m_sampler_cache) {
        if (sampler) {
            device.destroySampler(sampler);
        }
    }
    m_sampler_cache.clear();

    m_backend = nullptr;

    s_initialized = false;

    MF_INFO(Journal::Component::Portal, Journal::Context::ImageProcessing,
        "SamplerForge shutdown complete");
}

vk::Sampler SamplerForge::get_or_create(const SamplerConfig& config)
{
    if (!is_initialized()) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "SamplerForge not initialized");
        return nullptr;
    }

    size_t hash = hash_config(config);

    auto it = m_sampler_cache.find(hash);
    if (it != m_sampler_cache.end()) {
        return it->second;
    }

    vk::Sampler sampler = create_sampler(config);
    if (sampler) {
        m_sampler_cache[hash] = sampler;
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Created and cached sampler (hash: {}, total: {})",
            hash, m_sampler_cache.size());
    }

    return sampler;
}

vk::Sampler SamplerForge::get_default_linear()
{
    SamplerConfig config;
    config.mag_filter = FilterMode::LINEAR;
    config.min_filter = FilterMode::LINEAR;
    config.address_mode_u = AddressMode::REPEAT;
    config.address_mode_v = AddressMode::REPEAT;
    config.address_mode_w = AddressMode::REPEAT;
    config.max_anisotropy = 1.0F;
    return get_or_create(config);
}

vk::Sampler SamplerForge::get_default_nearest()
{
    SamplerConfig config;
    config.mag_filter = FilterMode::NEAREST;
    config.min_filter = FilterMode::NEAREST;
    config.address_mode_u = AddressMode::CLAMP_TO_EDGE;
    config.address_mode_v = AddressMode::CLAMP_TO_EDGE;
    config.address_mode_w = AddressMode::CLAMP_TO_EDGE;
    config.max_anisotropy = 1.0F;
    return get_or_create(config);
}

vk::Sampler SamplerForge::get_anisotropic(float max_anisotropy)
{
    SamplerConfig config;
    config.mag_filter = FilterMode::LINEAR;
    config.min_filter = FilterMode::LINEAR;
    config.address_mode_u = AddressMode::REPEAT;
    config.address_mode_v = AddressMode::REPEAT;
    config.address_mode_w = AddressMode::REPEAT;
    config.max_anisotropy = std::clamp(max_anisotropy, 1.0F, 16.0F);
    return get_or_create(config);
}

void SamplerForge::destroy_sampler(vk::Sampler sampler)
{
    if (!is_initialized() || !sampler) {
        return;
    }

    for (auto it = m_sampler_cache.begin(); it != m_sampler_cache.end(); ++it) {
        if (it->second == sampler) {
            auto device = m_backend->get_context().get_device();
            device.destroySampler(sampler);
            m_sampler_cache.erase(it);
            MF_DEBUG(Journal::Component::Portal, Journal::Context::ImageProcessing,
                "Destroyed sampler (remaining: {})", m_sampler_cache.size());
            return;
        }
    }
}

vk::Sampler SamplerForge::create_sampler(const SamplerConfig& config)
{
    auto device = m_backend->get_context().get_device();
    auto physical_device = m_backend->get_context().get_physical_device();

    vk::SamplerCreateInfo sampler_info {};
    sampler_info.magFilter = to_vk_filter(config.mag_filter);
    sampler_info.minFilter = to_vk_filter(config.min_filter);

    sampler_info.addressModeU = to_vk_address_mode(config.address_mode_u);
    sampler_info.addressModeV = to_vk_address_mode(config.address_mode_v);
    sampler_info.addressModeW = to_vk_address_mode(config.address_mode_w);

    if (config.max_anisotropy > 1.0F) {
        auto properties = physical_device.getProperties();
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = std::min(
            config.max_anisotropy,
            properties.limits.maxSamplerAnisotropy);
    } else {
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 1.0F;
    }

    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;

    sampler_info.unnormalizedCoordinates = VK_FALSE;

    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;

    if (config.enable_mipmaps) {
        sampler_info.mipmapMode = (config.min_filter == FilterMode::LINEAR)
            ? vk::SamplerMipmapMode::eLinear
            : vk::SamplerMipmapMode::eNearest;
        sampler_info.minLod = 0.0F;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE;
        sampler_info.mipLodBias = 0.0F;
    } else {
        sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
        sampler_info.minLod = 0.0F;
        sampler_info.maxLod = 0.0F;
        sampler_info.mipLodBias = 0.0F;
    }

    try {
        vk::Sampler sampler = device.createSampler(sampler_info);
        MF_DEBUG(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Created sampler: mag={}, min={}, aniso={}",
            vk::to_string(sampler_info.magFilter),
            vk::to_string(sampler_info.minFilter),
            sampler_info.maxAnisotropy);
        return sampler;
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::ImageProcessing,
            "Failed to create sampler: {}", e.what());
        return nullptr;
    }
}

size_t SamplerForge::hash_config(const SamplerConfig& config)
{
    size_t hash = 0;
    hash ^= std::hash<int> {}(static_cast<int>(config.mag_filter)) << 0;
    hash ^= std::hash<int> {}(static_cast<int>(config.min_filter)) << 4;
    hash ^= std::hash<int> {}(static_cast<int>(config.address_mode_u)) << 8;
    hash ^= std::hash<int> {}(static_cast<int>(config.address_mode_v)) << 12;
    hash ^= std::hash<int> {}(static_cast<int>(config.address_mode_w)) << 16;
    hash ^= std::hash<float> {}(config.max_anisotropy) << 20;
    hash ^= std::hash<bool> {}(config.enable_mipmaps) << 24;
    return hash;
}

vk::Filter SamplerForge::to_vk_filter(FilterMode mode)
{
    switch (mode) {
    case FilterMode::NEAREST:
        return vk::Filter::eNearest;
    case FilterMode::LINEAR:
        return vk::Filter::eLinear;
    case FilterMode::CUBIC:
        return vk::Filter::eCubicEXT;
    default:
        return vk::Filter::eLinear;
    }
}

vk::SamplerAddressMode SamplerForge::to_vk_address_mode(AddressMode mode)
{
    switch (mode) {
    case AddressMode::REPEAT:
        return vk::SamplerAddressMode::eRepeat;
    case AddressMode::MIRRORED_REPEAT:
        return vk::SamplerAddressMode::eMirroredRepeat;
    case AddressMode::CLAMP_TO_EDGE:
        return vk::SamplerAddressMode::eClampToEdge;
    case AddressMode::CLAMP_TO_BORDER:
        return vk::SamplerAddressMode::eClampToBorder;
    default:
        return vk::SamplerAddressMode::eRepeat;
    }
}

} // namespace MayaFlux::Portal::Graphics
