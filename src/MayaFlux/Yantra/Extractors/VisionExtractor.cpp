#include "VisionExtractor.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"
#include "MayaFlux/Kakshya/Source/WindowContainer.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

namespace MayaFlux::Yantra {

namespace {

    struct CropPC {
        float src_x, src_y, src_w, src_h;
        uint32_t out_w, out_h;
    };

    struct RegionSamplePC {
        uint32_t px, py, pw, ph;
    };

    struct PatchExtractPC {
        uint32_t patch_w, patch_h, count, src_w, src_h;
    };

} // namespace

VisionExtractor::VisionExtractor(VisionExtractMode mode)
    : Base([this](const input_type& input) { return run_operation(input); })
    , m_mode(mode)
    , m_crop_ctx(std::make_shared<TextureExecutionContext>(
          GpuComputeConfig {
              .shader_path = "vision_crop.comp.spv",
              .workgroup_size = { 16, 16, 1 },
              .push_constant_size = sizeof(CropPC) },
          Portal::Graphics::ImageFormat::RGBA8,
          TextureExecutionContext::OutputMode::IMAGE))
    , m_sample_ctx(std::make_shared<TextureExecutionContext>(
          GpuComputeConfig {
              .shader_path = "region_sample.comp.spv",
              .workgroup_size = { 256, 1, 1 },
              .push_constant_size = sizeof(RegionSamplePC) },
          Portal::Graphics::ImageFormat::RGBA8,
          TextureExecutionContext::OutputMode::SCALAR,
          1,
          std::vector<GpuBufferBinding> {
              { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::OUTPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 } }))
    , m_patch_ctx(std::make_shared<TextureExecutionContext>(
          GpuComputeConfig {
              .shader_path = "vision_patch_extract.comp.spv",
              .workgroup_size = { 16, 16, 1 },
              .push_constant_size = sizeof(PatchExtractPC) },
          Portal::Graphics::ImageFormat::RGBA8,
          TextureExecutionContext::OutputMode::IMAGE,
          1,
          std::vector<GpuBufferBinding> {
              { .set = 0, .binding = 2, .direction = GpuBufferBinding::Direction::INPUT, .element_type = GpuBufferBinding::ElementType::FLOAT32 } }))
{
}

std::shared_ptr<Core::VKImage> VisionExtractor::crop(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::BoundingBox& region,
    uint32_t out_w, uint32_t out_h)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();

    m_crop_ctx->stage_image(source);
    m_crop_ctx->set_push_constants(CropPC {
        .src_x = region.x, .src_y = region.y, .src_w = region.w, .src_h = region.h,
        .out_w = out_w, .out_h = out_h });
    m_crop_ctx->prepare_output_image(out_w, out_h);
    m_crop_ctx->set_output_dimensions(out_w, out_h);

    const auto fence = m_crop_ctx->dispatch_async({});
    m_crop_ctx->clear_output_dimensions();
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);

    return m_crop_ctx->get_output_image(0);
}

std::shared_ptr<Core::VKImage> VisionExtractor::mask(
    VisionGpuExecutor& executor,
    const std::shared_ptr<Core::VKImage>& source,
    uint32_t target_label,
    uint32_t w, uint32_t h)
{
    return executor.select_label(source, target_label, w, h);
}

Kinesis::Vision::FieldSample VisionExtractor::sample(
    const std::shared_ptr<Core::VKImage>& source,
    const Kinesis::Vision::BoundingBox& region,
    uint32_t source_w, uint32_t source_h)
{
    auto& foundry = Portal::Graphics::get_shader_foundry();

    const auto px = static_cast<uint32_t>(region.x * static_cast<float>(source_w));
    const auto py = static_cast<uint32_t>(region.y * static_cast<float>(source_h));
    const auto pw = std::max<uint32_t>(1U, static_cast<uint32_t>(region.w * static_cast<float>(source_w)));
    const auto ph = std::max<uint32_t>(1U, static_cast<uint32_t>(region.h * static_cast<float>(source_h)));

    m_sample_ctx->stage_image(source);
    m_sample_ctx->set_push_constants(RegionSamplePC { .px = px, .py = py, .pw = pw, .ph = ph });
    m_sample_ctx->set_output_size(2, 3 * sizeof(glm::vec4));
    m_sample_ctx->set_output_dimensions(1, 1);

    const auto fence = m_sample_ctx->dispatch_async({});
    m_sample_ctx->clear_output_dimensions();
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);

    const auto gpu_result = m_sample_ctx->collect_result();

    Kinesis::Vision::FieldSample out;
    if (auto it = gpu_result.aux.find(2); it != gpu_result.aux.end() && it->second.size() >= 3 * sizeof(glm::vec4)) {
        std::array<glm::vec4, 3> raw {};
        std::memcpy(raw.data(), it->second.data(), sizeof(raw));
        out.mean = raw[0];
        out.min_val = raw[1];
        out.max_val = raw[2];
    }
    return out;
}

std::shared_ptr<Core::VKImage> VisionExtractor::patches(
    const std::shared_ptr<Core::VKImage>& source,
    const std::vector<glm::vec2>& centers,
    uint32_t patch_w, uint32_t patch_h,
    uint32_t source_w, uint32_t source_h)
{
    if (centers.empty())
        return nullptr;

    auto& foundry = Portal::Graphics::get_shader_foundry();
    const auto count = static_cast<uint32_t>(centers.size());
    const auto out_w = patch_w * count;

    m_patch_ctx->stage_image(source);
    m_patch_ctx->set_binding_data(2, std::span<const float>(
        reinterpret_cast<const float*>(centers.data()), centers.size() * 2));
    m_patch_ctx->set_push_constants(PatchExtractPC {
        .patch_w = patch_w, .patch_h = patch_h, .count = count,
        .src_w = source_w, .src_h = source_h });
    m_patch_ctx->prepare_output_image(out_w, patch_h);
    m_patch_ctx->set_output_dimensions(out_w, patch_h);

    const auto fence = m_patch_ctx->dispatch_async({});
    m_patch_ctx->clear_output_dimensions();
    foundry.wait_for_fence(fence);
    foundry.release_fence(fence);

    return m_patch_ctx->get_output_image(0);
}

std::shared_ptr<Core::VKImage> VisionExtractor::resolve_image(
    const std::shared_ptr<Kakshya::SignalSourceContainer>& source,
    size_t index)
{
    if (!source)
        return nullptr;

    if (auto tc = std::dynamic_pointer_cast<Kakshya::TextureContainer>(source))
        return tc->to_image(static_cast<uint32_t>(index));

    if (auto wc = std::dynamic_pointer_cast<Kakshya::WindowContainer>(source))
        return wc->to_image();

    auto vc = std::dynamic_pointer_cast<Kakshya::VideoStreamContainer>(source);
    if (!vc)
        return nullptr;

    const uint32_t w = vc->get_width();
    const uint32_t h = vc->get_height();

    const void* raw = source->get_raw_data();
    if (!raw)
        return nullptr;

    if (!m_upload_image || m_upload_w != w || m_upload_h != h) {
        m_upload_image = Portal::Graphics::TextureLoom::instance().create_2d(
            w, h, Portal::Graphics::ImageFormat::RGBA8, nullptr);
        m_upload_w = w;
        m_upload_h = h;
    }
    if (!m_upload_image)
        return nullptr;

    Portal::Graphics::TextureLoom::instance().upload_data(m_upload_image, raw, m_upload_image->get_size_bytes());
    return m_upload_image;
}

VisionExtractor::output_type VisionExtractor::run_operation(const input_type& input)
{
    output_type output;
    output.metadata = input.metadata;

    if (!input.data) {
        output.metadata["error"] = std::string("VisionExtractor: missing container");
        return output;
    }

    const auto index = Kakshya::get_metadata_value<size_t>(input.metadata, "container_index").value_or(0);
    const auto image = resolve_image(input.data, index);
    if (!image) {
        output.metadata["error"] = std::string("VisionExtractor: could not resolve image from container");
        return output;
    }

    switch (m_mode) {
    case VisionExtractMode::Crop:
        output.metadata["vision_extraction"] = crop(image, m_region, m_out_w, m_out_h);
        break;
    case VisionExtractMode::Sample:
        output.metadata["vision_extraction"] = sample(image, m_region, image->get_width(), image->get_height());
        break;
    case VisionExtractMode::Patches:
        output.metadata["vision_extraction"] = patches(
            image, m_centers, m_patch_w, m_patch_h, image->get_width(), image->get_height());
        break;
    }

    return output;
}

} // namespace MayaFlux::Yantra
