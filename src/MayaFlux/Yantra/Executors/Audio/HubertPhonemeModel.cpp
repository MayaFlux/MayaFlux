#include "HubertPhonemeModel.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"
#include "MayaFlux/Yantra/Executors/TensorKernels/Stages.hpp"
#include "MayaFlux/Yantra/Executors/TensorKernels/Weights.hpp"

namespace MayaFlux::Yantra {

namespace {

    using TensorKernels::Activation;
    using TensorKernels::NormAffine;
    using TensorKernels::TensorRequirement;

    constexpr uint32_t k_feature_layers = 7;
    constexpr uint32_t k_features = 512;
    constexpr uint32_t k_width = HubertPhonemeModel::hidden_width;
    constexpr uint32_t k_expanded = 3072;
    constexpr uint32_t k_blocks = 12;
    constexpr uint32_t k_heads = 12;
    constexpr uint32_t k_head_width = k_width / k_heads;
    constexpr uint32_t k_position_kernel = 128;
    constexpr uint32_t k_position_groups = 16;
    constexpr uint32_t k_classes = HubertPhonemeModel::classes;
    constexpr float k_norm_epsilon = 1.0e-5F;
    constexpr double k_input_epsilon = 1.0e-7;

    constexpr std::array<uint32_t, k_feature_layers> k_feature_kernels { 10, 3, 3, 3, 3, 2, 2 };
    constexpr std::array<uint32_t, k_feature_layers> k_feature_strides { 5, 2, 2, 2, 2, 2, 2 };

    constexpr std::array<std::string_view, k_classes> k_labels {
        "AA", "AE", "AH", "AO", "AW", "AY", "B", "CH", "D", "DH", "EH", "ER", "EY", "F", "G", "HH", "IH", "IY", "JH", "K",
        "L", "M", "N", "NG", "OW", "OY", "P", "R", "S", "SH", "T", "TH", "UH", "UW", "V", "W", "Y", "Z", "ZH",
        "[UNK]", "[PAD]", "<s>", "</s>"
    };

    constexpr std::array<uint32_t, 15> k_vowels { 0, 1, 2, 3, 4, 5, 10, 11, 12, 16, 17, 24, 25, 32, 33 };

    enum Front : size_t {
        FeatureWeight0,
        FeatureNormWeight = FeatureWeight0 + k_feature_layers,
        FeatureNormBias,
        ProjectionNormWeight,
        ProjectionNormBias,
        ProjectionWeight,
        ProjectionBias,
        PositionBias,
        EncoderNormWeight,
        EncoderNormBias,
        HeadWeight,
        HeadBias,
        FrontCount
    };

    enum Block : size_t {
        QueryWeight,
        QueryBias,
        KeyWeight,
        KeyBias,
        ValueWeight,
        ValueBias,
        OutputWeight,
        OutputBias,
        NormWeight,
        NormBias,
        ExpandWeight,
        ExpandBias,
        ReduceWeight,
        ReduceBias,
        FinalNormWeight,
        FinalNormBias,
        BlockCount
    };

    enum Position : size_t {
        PositionGain,
        PositionDirection,
        PositionCount
    };

    enum Buffer : uint32_t {
        Audio,
        WideA,
        WideB,
        Stats,
        Hidden,
        Positional,
        Query,
        Key,
        Value,
        Scores,
        Expanded,
        Posterior,
        WeightsFront,
        WeightsPosition,
        WeightsBlock0
    };

    TensorKernels::Conv1d feature_layer(uint32_t index)
    {
        return {
            .c_in = index == 0 ? 1U : k_features,
            .c_out = k_features,
            .kernel = k_feature_kernels[index],
            .stride = k_feature_strides[index],
            .activation = index == 0 ? Activation::None : Activation::Gelu,
        };
    }

    std::vector<TensorRequirement> front_specs()
    {
        std::vector<TensorRequirement> specs;
        specs.reserve(FrontCount);
        for (uint32_t i = 0; i < k_feature_layers; ++i) {
            specs.push_back({ std::format("hubert.feature_extractor.conv_layers.{}.conv.weight", i),
                { k_features, i == 0 ? 1U : k_features, k_feature_kernels[i] } });
        }
        specs.push_back({ "hubert.feature_extractor.conv_layers.0.layer_norm.weight", { k_features } });
        specs.push_back({ "hubert.feature_extractor.conv_layers.0.layer_norm.bias", { k_features } });
        specs.push_back({ "hubert.feature_projection.layer_norm.weight", { k_features } });
        specs.push_back({ "hubert.feature_projection.layer_norm.bias", { k_features } });
        specs.push_back({ "hubert.feature_projection.projection.weight", { k_width, k_features } });
        specs.push_back({ "hubert.feature_projection.projection.bias", { k_width } });
        specs.push_back({ "hubert.encoder.pos_conv_embed.conv.bias", { k_width } });
        specs.push_back({ "hubert.encoder.layer_norm.weight", { k_width } });
        specs.push_back({ "hubert.encoder.layer_norm.bias", { k_width } });
        specs.push_back({ "lm_head.weight", { k_classes, k_width } });
        specs.push_back({ "lm_head.bias", { k_classes } });
        return specs;
    }

    std::vector<TensorRequirement> block_specs(uint32_t block)
    {
        const std::string prefix = std::format("hubert.encoder.layers.{}.", block);
        const auto dense = [&](std::vector<TensorRequirement>& specs, std::string_view name, uint32_t out, uint32_t in) {
            specs.push_back({ std::format("{}{}.weight", prefix, name), { out, in } });
            specs.push_back({ std::format("{}{}.bias", prefix, name), { out } });
        };
        const auto norm = [&](std::vector<TensorRequirement>& specs, std::string_view name) {
            specs.push_back({ std::format("{}{}.weight", prefix, name), { k_width } });
            specs.push_back({ std::format("{}{}.bias", prefix, name), { k_width } });
        };
        std::vector<TensorRequirement> specs;
        specs.reserve(BlockCount);
        dense(specs, "attention.q_proj", k_width, k_width);
        dense(specs, "attention.k_proj", k_width, k_width);
        dense(specs, "attention.v_proj", k_width, k_width);
        dense(specs, "attention.out_proj", k_width, k_width);
        norm(specs, "layer_norm");
        dense(specs, "feed_forward.intermediate_dense", k_expanded, k_width);
        dense(specs, "feed_forward.output_dense", k_width, k_expanded);
        norm(specs, "final_layer_norm");
        return specs;
    }

    std::vector<TensorRequirement> position_specs()
    {
        return {
            { "hubert.encoder.pos_conv_embed.conv.weight_g", { 1, 1, k_position_kernel } },
            { "hubert.encoder.pos_conv_embed.conv.weight_v", { k_width, k_width / k_position_groups, k_position_kernel } },
        };
    }

    TensorKernels::PackedWeights pack(const Kakshya::TensorSet& weights, const std::vector<TensorRequirement>& specs)
    {
        auto packed = TensorKernels::pack_weights(weights, specs);
        if (!packed) {
            error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "HubertPhonemeModel: {}", packed.error());
        }
        return std::move(*packed);
    }

    /**
     * The position weights the convolution runs with: each kernel tap's direction scaled to that tap's gain, the
     * norm of a tap taken over every output and input channel.
     */
    std::vector<float> fold_position_weights(const TensorKernels::PackedWeights& packed)
    {
        const size_t rows = packed.offsets.size() > PositionDirection
            ? (packed.values.size() - packed.offsets[PositionDirection]) / k_position_kernel
            : 0;
        const float* gain = packed.values.data() + packed.offsets[PositionGain];
        const float* direction = packed.values.data() + packed.offsets[PositionDirection];

        std::array<double, k_position_kernel> norms {};
        for (size_t row = 0; row < rows; ++row) {
            for (size_t tap = 0; tap < k_position_kernel; ++tap) {
                const double value = direction[row * k_position_kernel + tap];
                norms[tap] += value * value;
            }
        }
        std::vector<float> folded(rows * k_position_kernel);
        for (size_t row = 0; row < rows; ++row) {
            for (size_t tap = 0; tap < k_position_kernel; ++tap) {
                const double norm = std::sqrt(norms[tap]);
                folded[row * k_position_kernel + tap] = norm > 0.0
                    ? static_cast<float>(static_cast<double>(gain[tap]) * direction[row * k_position_kernel + tap] / norm)
                    : 0.0F;
            }
        }
        return folded;
    }

    std::array<uint32_t, k_feature_layers> feature_lengths(size_t samples)
    {
        std::array<uint32_t, k_feature_layers> lengths {};
        auto length = static_cast<uint32_t>(samples);
        for (uint32_t i = 0; i < k_feature_layers; ++i) {
            length = TensorKernels::conv1d_length(feature_layer(i), length);
            lengths[i] = length;
        }
        return lengths;
    }

    std::vector<float> frame_major(const std::vector<float>& rows, size_t channels, size_t frames)
    {
        std::vector<float> out(rows.size());
        for (size_t channel = 0; channel < channels; ++channel) {
            for (size_t frame = 0; frame < frames; ++frame) {
                out[frame * channels + channel] = rows[channel * frames + frame];
            }
        }
        return out;
    }

}

struct HubertPhonemeModel::Impl {
    std::shared_ptr<GpuDispatchRecorder> recording;
    std::unique_ptr<ShaderExecutionContext<>> context;
    TensorKernels::PackedWeights front;
    std::array<TensorKernels::PackedWeights, k_blocks> blocks;
    size_t resident { 0 };

    Impl(const Kakshya::TensorSet& weights, std::shared_ptr<GpuDispatchRecorder> shared)
        : recording(shared ? std::move(shared) : std::make_shared<GpuDispatchRecorder>(true))
        , context(std::make_unique<ShaderExecutionContext<>>(
              GpuComputeConfig { .shader_path = TensorKernels::conv1d_shader, .workgroup_size = { TensorKernels::element_workgroup, 1, 1 }, .push_constant_size = sizeof(TensorKernels::Conv1dPC) }))
    {
        context->set_recording(recording);
        if (!context->ensure_gpu_ready()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "HubertPhonemeModel needs a graphics device and none is available");
        }

        front = pack(weights, front_specs());
        TensorKernels::upload_packed_weights(*context, 0, WeightsFront, front);
        front.values = std::vector<float> {};

        for (uint32_t block = 0; block < k_blocks; ++block) {
            blocks[block] = pack(weights, block_specs(block));
            TensorKernels::upload_packed_weights(*context, 0, WeightsBlock0 + block, blocks[block]);
            blocks[block].values = std::vector<float> {};
        }

        const auto folded = fold_position_weights(pack(weights, position_specs()));
        context->ensure_shared_buffer(0, WeightsPosition, folded.size(), GpuBufferBinding::ElementType::FLOAT32,
            Portal::Graphics::BufferUsageHint::COMPUTE);
        context->upload_shared_raw(0, WeightsPosition, reinterpret_cast<const uint8_t*>(folded.data()), folded.size() * sizeof(float));
    }

    void ensure_capacity(size_t samples, const std::array<uint32_t, k_feature_layers>& lengths)
    {
        using Portal::Graphics::BufferUsageHint;
        constexpr auto type = GpuBufferBinding::ElementType::FLOAT32;
        const size_t frames = lengths.back();
        context->ensure_shared_buffer(0, Audio, samples, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, WideA, static_cast<size_t>(k_features) * lengths[0], type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, WideB, static_cast<size_t>(k_features) * lengths[1], type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Stats, 2 * std::max<size_t>(k_features, frames), type, BufferUsageHint::COMPUTE);
        for (const uint32_t buffer : { Hidden, Positional, Query, Key, Value }) {
            context->ensure_shared_buffer(0, buffer, k_width * frames, type, BufferUsageHint::COMPUTE);
        }
        context->ensure_shared_buffer(0, Scores, k_heads * frames * frames, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Expanded, k_expanded * frames, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Posterior, k_classes * frames, type);
    }

    [[nodiscard]] uint64_t address(uint32_t buffer) const
    {
        return context->shared_buffer_address(0, buffer);
    }

    void upload_normalised(std::span<const float> samples)
    {
        double sum = 0.0;
        for (const float sample : samples) {
            sum += sample;
        }
        const double mean = sum / static_cast<double>(samples.size());
        double square = 0.0;
        for (const float sample : samples) {
            square += (sample - mean) * (sample - mean);
        }
        const double scale = 1.0 / std::sqrt(square / static_cast<double>(samples.size()) + k_input_epsilon);

        std::vector<float> normalised(samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            normalised[i] = static_cast<float>((samples[i] - mean) * scale);
        }
        context->upload_shared_raw(0, Audio, reinterpret_cast<const uint8_t*>(normalised.data()), normalised.size() * sizeof(float));
    }

    [[nodiscard]] std::vector<float> run(std::span<const float> samples)
    {
        const auto lengths = feature_lengths(samples.size());
        const uint32_t frames = lengths.back();
        ensure_capacity(samples.size(), lengths);
        upload_normalised(samples);

        const auto front_at = [&](size_t slot) { return front.address(address(WeightsFront), slot); };
        const auto block_at = [&](uint32_t block, size_t slot) { return blocks[block].address(address(WeightsBlock0 + block), slot); };

        std::vector<DependencyStage> stages;
        const auto dense = [&](uint64_t src, uint32_t in, uint64_t weight, uint64_t bias, uint64_t dst, uint32_t out, Activation activation = Activation::None) {
            stages.push_back(TensorKernels::conv1d_stage({ .c_in = in, .c_out = out, .kernel = 1, .activation = activation }, src, weight, bias, dst, 1, frames));
        };
        const auto layer_norm = [&](uint64_t data, uint32_t channels, uint64_t weight, uint64_t bias) {
            stages.push_back(TensorKernels::normalize_stats_stage(data, address(Stats), 1, channels, frames, k_norm_epsilon));
            stages.push_back(TensorKernels::normalize_apply_stage(data, address(Stats), weight, bias, data, 1, channels, frames, NormAffine::Along));
        };

        uint64_t features = address(Audio);
        auto length = static_cast<uint32_t>(samples.size());
        for (uint32_t i = 0; i < k_feature_layers; ++i) {
            const uint64_t dst = address(i % 2 == 0 ? WideA : WideB);
            stages.push_back(TensorKernels::conv1d_stage(feature_layer(i), features, front_at(FeatureWeight0 + i), 0, dst, 1, length));
            if (i == 0) {
                stages.push_back(TensorKernels::normalize_stats_stage(dst, address(Stats), k_features, lengths[0], 1, k_norm_epsilon));
                stages.push_back(TensorKernels::normalize_apply_stage(dst, address(Stats), front_at(FeatureNormWeight), front_at(FeatureNormBias),
                    dst, k_features, lengths[0], 1, NormAffine::Outer, Activation::Gelu));
            }
            features = dst;
            length = lengths[i];
        }

        const uint64_t hidden = address(Hidden);
        layer_norm(features, k_features, front_at(ProjectionNormWeight), front_at(ProjectionNormBias));
        dense(features, k_features, front_at(ProjectionWeight), front_at(ProjectionBias), hidden, k_width);

        stages.push_back(TensorKernels::grouped_conv1d_stage(
            { .c_in = k_width, .c_out = k_width, .kernel = k_position_kernel, .groups = k_position_groups, .pad = k_position_kernel / 2, .trim = 1, .activation = Activation::Gelu },
            hidden, address(WeightsPosition), front_at(PositionBias), address(Positional), 1, frames));
        stages.push_back(TensorKernels::add_stage(hidden, address(Positional), hidden, k_width * frames));
        layer_norm(hidden, k_width, front_at(EncoderNormWeight), front_at(EncoderNormBias));

        const float score_scale = 1.0F / std::sqrt(static_cast<float>(k_head_width));
        for (uint32_t block = 0; block < k_blocks; ++block) {
            dense(hidden, k_width, block_at(block, QueryWeight), block_at(block, QueryBias), address(Query), k_width);
            dense(hidden, k_width, block_at(block, KeyWeight), block_at(block, KeyBias), address(Key), k_width);
            dense(hidden, k_width, block_at(block, ValueWeight), block_at(block, ValueBias), address(Value), k_width);
            stages.push_back(TensorKernels::attention_scores_stage(address(Query), address(Key), address(Scores), k_heads, k_head_width, frames, score_scale));
            stages.push_back(TensorKernels::softmax_stage(address(Scores), k_heads * frames, frames, 1));
            stages.push_back(TensorKernels::attention_mix_stage(address(Scores), address(Value), address(Query), k_heads, k_head_width, frames));
            dense(address(Query), k_width, block_at(block, OutputWeight), block_at(block, OutputBias), address(Key), k_width);
            stages.push_back(TensorKernels::add_stage(hidden, address(Key), hidden, k_width * frames));
            layer_norm(hidden, k_width, block_at(block, NormWeight), block_at(block, NormBias));

            dense(hidden, k_width, block_at(block, ExpandWeight), block_at(block, ExpandBias), address(Expanded), k_expanded, Activation::Gelu);
            dense(address(Expanded), k_expanded, block_at(block, ReduceWeight), block_at(block, ReduceBias), address(Key), k_width);
            stages.push_back(TensorKernels::add_stage(hidden, address(Key), hidden, k_width * frames));
            layer_norm(hidden, k_width, block_at(block, FinalNormWeight), block_at(block, FinalNormBias));
        }

        dense(hidden, k_width, front_at(HeadWeight), front_at(HeadBias), address(Posterior), k_classes);
        stages.push_back(TensorKernels::softmax_stage(address(Posterior), 1, k_classes, frames));

        context->record_stages(stages);
        resident = frames;

        std::vector<float> rows(static_cast<size_t>(k_classes) * frames);
        context->download_shared(0, Posterior, rows.data(), rows.size() * sizeof(float));
        return frame_major(rows, k_classes, frames);
    }
};

std::string HubertPhonemeModel::diagnose(const Kakshya::TensorSet& weights)
{
    if (std::string reason = TensorKernels::check_tensors(weights, front_specs()); !reason.empty()) {
        return reason;
    }
    if (std::string reason = TensorKernels::check_tensors(weights, position_specs()); !reason.empty()) {
        return reason;
    }
    for (uint32_t block = 0; block < k_blocks; ++block) {
        if (std::string reason = TensorKernels::check_tensors(weights, block_specs(block)); !reason.empty()) {
            return reason;
        }
    }
    return {};
}

std::string_view HubertPhonemeModel::label(uint32_t index) noexcept
{
    return index < k_labels.size() ? k_labels[index] : std::string_view {};
}

bool HubertPhonemeModel::is_vowel(uint32_t index) noexcept
{
    return std::ranges::find(k_vowels, index) != k_vowels.end();
}

uint32_t HubertPhonemeModel::frame_count(size_t samples) noexcept
{
    if (samples < frame_span || samples > std::numeric_limits<uint32_t>::max()) {
        return 0;
    }
    return feature_lengths(samples).back();
}

HubertPhonemeModel::HubertPhonemeModel(const Kakshya::TensorSet& weights)
    : HubertPhonemeModel(weights, nullptr)
{
}

HubertPhonemeModel::HubertPhonemeModel(const Kakshya::TensorSet& weights, std::shared_ptr<GpuDispatchRecorder> recording)
{
    if (const std::string reason = diagnose(weights); !reason.empty()) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "HubertPhonemeModel: {}", reason);
    }
    m_impl = std::make_unique<Impl>(weights, std::move(recording));
}

HubertPhonemeModel::~HubertPhonemeModel() = default;

std::vector<float> HubertPhonemeModel::process(std::span<const float> samples)
{
    if (samples.size() > max_samples) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(),
            "HubertPhonemeModel: an utterance of {} samples is longer than the {} one call accepts; run it in pieces, for instance the intervals a voice activity analysis gives",
            samples.size(), max_samples);
    }
    if (samples.size() < frame_span) {
        m_impl->resident = 0;
        return {};
    }
    return m_impl->run(samples);
}

std::vector<float> HubertPhonemeModel::hidden_states()
{
    const size_t frames = m_impl->resident;
    if (frames == 0) {
        return {};
    }
    std::vector<float> rows(hidden_width * frames);
    m_impl->context->download_shared(0, Hidden, rows.data(), rows.size() * sizeof(float));
    return frame_major(rows, hidden_width, frames);
}

size_t HubertPhonemeModel::resident_frames() const noexcept
{
    return m_impl->resident;
}

HubertPhonemeModel::DeviceBuffer HubertPhonemeModel::posterior_buffer()
{
    m_impl->context->ensure_shared_buffer(0, Posterior, k_classes, GpuBufferBinding::ElementType::FLOAT32);
    return { .context = m_impl->context.get(), .set = 0, .binding = Posterior };
}

HubertPhonemeModel::DeviceBuffer HubertPhonemeModel::hidden_buffer()
{
    m_impl->context->ensure_shared_buffer(0, Hidden, k_width, GpuBufferBinding::ElementType::FLOAT32,
        Portal::Graphics::BufferUsageHint::COMPUTE);
    return { .context = m_impl->context.get(), .set = 0, .binding = Hidden };
}

} // namespace MayaFlux::Yantra
