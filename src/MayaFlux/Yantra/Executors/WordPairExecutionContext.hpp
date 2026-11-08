#pragma once

#include "ShaderExecutionContext.hpp"

#include "MayaFlux/Kinesis/Discrete/OrderKey.hpp"

/**
 * @file WordPairExecutionContext.hpp
 * @brief Runs a shader on doubles carried as two 32-bit words, with no fp64 on the device.
 */

namespace MayaFlux::Yantra {

/**
 * @class WordPairExecutionContext
 * @brief Shader context whose numeric input is staged as a uvec2 per double.
 *
 * Each double is encoded as one 64-bit word, low word first, so the shader sees
 * a VEC2_U32 binding and needs neither shaderFloat64 nor lossy narrowing to
 * float. The word is the double's order key, whose unsigned order is the
 * numeric order, or its IEEE 754 bits unchanged. The context stages any
 * binding from a span of doubles and keeps the primary output of a dispatch
 * as raw words for the caller to interpret. Bindings, push constants and multipass come from
 * ShaderExecutionContext.
 */
class WordPairExecutionContext : public ShaderExecutionContext<> {
public:
    /**
     * @param config Pipeline of the shader.
     * @param bindings Binding list. The first output or in/out binding is the primary output.
     * @param order_key True to stage order keys, false to stage the IEEE 754 bits unchanged.
     * @param name Name used in diagnostics.
     */
    WordPairExecutionContext(
        GpuComputeConfig config,
        std::vector<GpuBufferBinding> bindings,
        bool order_key,
        std::string name = "WordPairExecutionContext")
        : ShaderExecutionContext<>(std::move(config), std::move(bindings), std::move(name))
        , m_order_key(order_key)
    {
    }

    [[nodiscard]] bool order_keyed() const noexcept { return m_order_key; }

    /**
     * @brief Encode values and stage them as the data of a binding.
     * @param binding Binding that receives the words.
     * @param values Doubles to encode.
     * @param padded_count Word count to stage. Words past values are all ones, the largest key.
     * @param complement Complements every encoded word, which reverses an order key's order.
     */
    void stage_words(uint32_t binding, std::span<const double> values, size_t padded_count = 0, bool complement = false)
    {
        std::vector<uint64_t> words(std::max(padded_count, values.size()), ~uint64_t { 0 });
        for (size_t i = 0; i < values.size(); ++i) {
            const uint64_t word = m_order_key
                ? Kinesis::Discrete::order_key(values[i])
                : std::bit_cast<uint64_t>(values[i]);
            words[i] = complement ? ~word : word;
        }
        set_binding_data(binding, words);
    }

    /**
     * @brief The primary output of the last dispatch as 32-bit words, empty if it produced none.
     */
    [[nodiscard]] std::span<const uint32_t> primary_words() const noexcept { return m_primary_words; }

protected:
    /**
     * @brief Keeps the primary output as words; the context returns no Datum.
     */
    output_type collect_gpu_outputs(
        const GpuChannelResult& raw,
        const std::vector<std::vector<double>>& /*channels*/,
        const DataStructureInfo& /*structure_info*/) override
    {
        m_primary_words.resize(raw.primary.size());
        std::memcpy(m_primary_words.data(), raw.primary.data(), raw.primary.size() * sizeof(float));
        return output_type {};
    }

private:
    bool m_order_key;
    std::vector<uint32_t> m_primary_words;
};

} // namespace MayaFlux::Yantra
