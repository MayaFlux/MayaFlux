#pragma once

#include "MayaFlux/Kakshya/NDData/Tensor.hpp"
#include "MayaFlux/Kakshya/NDData/TensorConvert.hpp"
#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"

/**
 * @file Weights.hpp
 * @brief Checking a set of tensors against what a network needs, and packing it into one float32 buffer.
 *
 * A network built from the tensor kernels lists the tensors it needs by name
 * and shape, in the order its layers use them. check_tensors says whether a
 * set holds them, pack_weights converts them to float32 and lays them out one
 * after another, and upload_packed_weights puts the result on a context, where
 * PackedWeights::address gives each tensor's device address for a stage.
 */

namespace MayaFlux::Yantra::TensorKernels {

/**
 * @struct TensorRequirement
 * @brief One tensor a network needs.
 */
struct TensorRequirement {
    std::string name;
    std::vector<uint64_t> shape;
};

namespace detail {

    [[nodiscard]] inline std::string shape_text(const std::vector<uint64_t>& shape)
    {
        std::string text = "[";
        for (size_t i = 0; i < shape.size(); ++i) {
            text += (i > 0 ? ", " : "") + std::to_string(shape[i]);
        }
        return text + "]";
    }

    [[nodiscard]] inline size_t element_count(const std::vector<uint64_t>& shape)
    {
        size_t count = 1;
        for (const uint64_t extent : shape) {
            count *= extent;
        }
        return count;
    }

}

/**
 * @brief Whether a set holds every required tensor with the required shape.
 * @param weights Candidate tensors.
 * @param required What the network needs.
 * @return Empty when it does, otherwise the first missing tensor or shape mismatch, in the order of required.
 */
[[nodiscard]] inline std::string check_tensors(const Kakshya::TensorSet& weights, std::span<const TensorRequirement> required)
{
    for (const auto& need : required) {
        const auto* tensor = weights.find(need.name);
        if (tensor == nullptr) {
            return std::format("missing tensor '{}'", need.name);
        }
        if (tensor->spec().shape != need.shape) {
            return std::format("tensor '{}' has shape {} but {} is required",
                need.name, detail::shape_text(tensor->spec().shape), detail::shape_text(need.shape));
        }
    }
    return {};
}

/**
 * @struct PackedWeights
 * @brief The required tensors as float32 in one run, in the order they were required.
 */
struct PackedWeights {
    std::vector<float> values;
    /** Element offset of each required tensor in values. */
    std::vector<size_t> offsets;

    /**
     * @brief Device address of one tensor, given the address of the uploaded buffer.
     * @param buffer Device address of the buffer upload_packed_weights filled.
     * @param index Position of the tensor in the requirements.
     */
    [[nodiscard]] uint64_t address(uint64_t buffer, size_t index) const noexcept
    {
        return buffer + offsets[index] * sizeof(float);
    }
};

/**
 * @brief Convert the required tensors to float32 and lay them out in the order of required.
 * @param weights Tensors to read. Other element types than float32 are converted.
 * @param required What the network needs.
 * @return The packed values, or the reason: what check_tensors says, or the tensor that cannot become float32.
 */
[[nodiscard]] inline std::expected<PackedWeights, std::string> pack_weights(
    const Kakshya::TensorSet& weights, std::span<const TensorRequirement> required)
{
    if (std::string reason = check_tensors(weights, required); !reason.empty()) {
        return std::unexpected(std::move(reason));
    }

    PackedWeights packed;
    packed.offsets.reserve(required.size());
    for (const auto& need : required) {
        const auto* tensor = weights.find(need.name);
        std::optional<std::vector<float>> values;
        if (tensor->spec().dtype == Kakshya::TensorDType::of<float>()) {
            values = tensor->to_vector<float>();
        } else if (const auto view = tensor->access()) {
            auto converted = Kakshya::convert_tensor(*view, Kakshya::TensorDType::of<float>());
            if (!converted) {
                return std::unexpected(std::format("tensor '{}' cannot become float32: {}", need.name, converted.error()));
            }
            values = converted->to_vector<float>();
        }
        if (!values || values->size() != detail::element_count(need.shape)) {
            return std::unexpected(std::format("tensor '{}' could not be read as float32", need.name));
        }
        packed.offsets.push_back(packed.values.size());
        packed.values.insert(packed.values.end(), values->begin(), values->end());
    }
    return packed;
}

/**
 * @brief Allocate a shared buffer on a context and fill it with packed weights.
 * @param context The context whose shared buffer holds the weights.
 * @param set Descriptor set of the buffer.
 * @param binding Binding of the buffer.
 * @param packed The weights. The buffer is device local, so it is read by the kernels and not from the host.
 */
inline void upload_packed_weights(GpuDispatchCore& context, uint32_t set, size_t binding, const PackedWeights& packed)
{
    if (packed.values.empty()) {
        return;
    }
    context.ensure_shared_buffer(set, binding, packed.values.size(), GpuBufferBinding::ElementType::FLOAT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);
    context.upload_shared_raw(set, binding, reinterpret_cast<const uint8_t*>(packed.values.data()),
        packed.values.size() * sizeof(float));
}

} // namespace MayaFlux::Yantra::TensorKernels
