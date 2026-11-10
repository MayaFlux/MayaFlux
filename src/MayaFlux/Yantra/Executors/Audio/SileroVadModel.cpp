#include "SileroVadModel.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/TensorConvert.hpp"
#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"
#include "MayaFlux/Yantra/Executors/ShaderExecutionContext.hpp"

namespace MayaFlux::Yantra {

namespace {

    enum Slot : size_t {
        StftWeight,
        Conv1Weight,
        Conv1Bias,
        Conv2Weight,
        Conv2Bias,
        Conv3Weight,
        Conv3Bias,
        Conv4Weight,
        Conv4Bias,
        LstmInputWeight,
        LstmHiddenWeight,
        LstmInputBias,
        LstmHiddenBias,
        HeadWeight,
        HeadBias,
        SlotCount
    };

    struct SlotSpec {
        std::string_view name;
        std::vector<uint64_t> shape;
    };

    const std::array<SlotSpec, SlotCount>& slot_specs()
    {
        static const std::array<SlotSpec, SlotCount> specs { {
            { "stft_conv.weight", { 258, 1, 256 } },
            { "conv1.weight", { 128, 129, 3 } },
            { "conv1.bias", { 128 } },
            { "conv2.weight", { 64, 128, 3 } },
            { "conv2.bias", { 64 } },
            { "conv3.weight", { 64, 64, 3 } },
            { "conv3.bias", { 64 } },
            { "conv4.weight", { 128, 64, 3 } },
            { "conv4.bias", { 128 } },
            { "lstm_cell.weight_ih", { 512, 128 } },
            { "lstm_cell.weight_hh", { 512, 128 } },
            { "lstm_cell.bias_ih", { 512 } },
            { "lstm_cell.bias_hh", { 512 } },
            { "final_conv.weight", { 1, 128, 1 } },
            { "final_conv.bias", { 1 } },
        } };
        return specs;
    }

    enum Buffer : uint32_t {
        Audio,
        Frames,
        Transform,
        Magnitude,
        Encoded1,
        Encoded2,
        Encoded3,
        Encoded4,
        Hidden,
        Probability,
        Weights,
        State
    };

    constexpr uint32_t k_frame = SileroVadModel::context_samples + SileroVadModel::chunk_samples + 64;
    constexpr uint32_t k_hidden = 128;
    constexpr size_t k_max_chunks = 4096;
    constexpr uint32_t k_flag_bias = 1;
    constexpr uint32_t k_flag_relu_input = 2;
    constexpr uint32_t k_relu = 1U << 2U;
    constexpr uint32_t k_sigmoid = 2U << 2U;

    struct FramePush {
        uint64_t samples;
        uint64_t frames;
        uint32_t chunks;
        uint32_t chunk_size;
        uint32_t context;
        uint32_t pad;
    };

    struct ConvPush {
        uint64_t src;
        uint64_t weights;
        uint64_t bias;
        uint64_t dst;
        uint32_t batch;
        uint32_t c_in;
        uint32_t t_in;
        uint32_t c_out;
        uint32_t t_out;
        uint32_t kernel_size;
        uint32_t stride;
        uint32_t pad;
        uint32_t flags;
        uint32_t reserved;
    };

    struct MagnitudePush {
        uint64_t src;
        uint64_t dst;
        uint32_t batch;
        uint32_t channels;
        uint32_t time;
        uint32_t reserved;
    };

    struct LstmPush {
        uint64_t x;
        uint64_t w_ih;
        uint64_t w_hh;
        uint64_t b_ih;
        uint64_t b_hh;
        uint64_t state;
        uint64_t hs;
        uint32_t steps;
        uint32_t reserved;
    };

    static_assert(sizeof(FramePush) == 32);
    static_assert(sizeof(ConvPush) == 72);
    static_assert(sizeof(MagnitudePush) == 32);
    static_assert(sizeof(LstmPush) == 64);

    std::string shape_text(const std::vector<uint64_t>& shape)
    {
        std::string text = "[";
        for (size_t i = 0; i < shape.size(); ++i) {
            text += (i > 0 ? ", " : "") + std::to_string(shape[i]);
        }
        return text + "]";
    }

    [[nodiscard]] uint32_t groups_for(size_t invocations, uint32_t local)
    {
        return static_cast<uint32_t>((invocations + local - 1) / local);
    }

}

struct SileroVadModel::Impl {
    std::shared_ptr<GpuDispatchRecorder> recording;
    std::unique_ptr<ShaderExecutionContext<>> context;
    std::array<size_t, SlotCount> offsets {};
    size_t total_weights {};
    size_t capacity {};
    std::vector<float> pending;
    std::vector<float> tail;

    explicit Impl(const Kakshya::TensorSet& weights)
        : recording(std::make_shared<GpuDispatchRecorder>(true))
        , context(std::make_unique<ShaderExecutionContext<>>(
              GpuComputeConfig { .shader_path = "nn_frame_audio.comp.spv", .workgroup_size = { 64, 1, 1 }, .push_constant_size = sizeof(FramePush) }))
        , tail(context_samples, 0.0F)
    {
        context->set_recording(recording);
        if (!context->ensure_gpu_ready()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "SileroVadModel needs a graphics device and none is available");
        }
        upload_weights(weights);
        context->ensure_shared_buffer(0, State, 2 * k_hidden, GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        clear_state();
    }

    void upload_weights(const Kakshya::TensorSet& weights)
    {
        size_t running = 0;
        for (size_t i = 0; i < SlotCount; ++i) {
            offsets[i] = running;
            size_t count = 1;
            for (const uint64_t extent : slot_specs()[i].shape) {
                count *= extent;
            }
            running += count;
        }
        total_weights = running;

        std::vector<float> floats;
        floats.reserve(total_weights);
        for (size_t i = 0; i < SlotCount; ++i) {
            const auto* tensor = weights.find(slot_specs()[i].name);
            std::optional<std::vector<float>> values;
            if (tensor->spec().dtype == Kakshya::TensorDType::of<float>()) {
                values = tensor->to_vector<float>();
            } else if (const auto view = tensor->access()) {
                auto converted = Kakshya::convert_tensor(*view, Kakshya::TensorDType::of<float>());
                if (!converted) {
                    error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                        std::source_location::current(), "SileroVadModel: tensor '{}' cannot become float32: {}",
                        slot_specs()[i].name, converted.error());
                }
                values = converted->to_vector<float>();
            }
            if (!values || values->size() != offsets_count(i)) {
                error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                    std::source_location::current(), "SileroVadModel: tensor '{}' could not be read as float32", slot_specs()[i].name);
            }
            floats.insert(floats.end(), values->begin(), values->end());
        }

        context->ensure_shared_buffer(0, Weights, total_weights, GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        context->upload_shared_raw(0, Weights, reinterpret_cast<const uint8_t*>(floats.data()), floats.size() * sizeof(float));
    }

    [[nodiscard]] size_t offsets_count(size_t slot) const
    {
        const size_t end = slot + 1 < SlotCount ? offsets[slot + 1] : total_weights;
        return end - offsets[slot];
    }

    void clear_state()
    {
        const std::vector<float> zeros(2 * k_hidden, 0.0F);
        context->upload_shared_raw(0, State, reinterpret_cast<const uint8_t*>(zeros.data()), zeros.size() * sizeof(float));
    }

    void ensure_capacity(size_t chunks)
    {
        if (chunks <= capacity) {
            return;
        }
        using Portal::Graphics::BufferUsageHint;
        constexpr auto type = GpuBufferBinding::ElementType::FLOAT32;
        context->ensure_shared_buffer(0, Audio, context_samples + chunks * chunk_samples, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Frames, chunks * k_frame, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Transform, chunks * 258 * 4, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Magnitude, chunks * 129 * 4, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Encoded1, chunks * 128 * 4, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Encoded2, chunks * 64 * 2, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Encoded3, chunks * 64, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Encoded4, chunks * k_hidden, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Hidden, chunks * k_hidden, type, BufferUsageHint::COMPUTE);
        context->ensure_shared_buffer(0, Probability, chunks, type);
        capacity = chunks;
    }

    [[nodiscard]] uint64_t address(uint32_t buffer) const
    {
        return context->shared_buffer_address(0, buffer);
    }

    [[nodiscard]] uint64_t weight_address(size_t slot) const
    {
        return address(Weights) + offsets[slot] * sizeof(float);
    }

    [[nodiscard]] static DependencyStage stage(const char* shader, uint32_t local, size_t push_size,
        uint32_t groups, std::function<void(GpuDispatchCore&)> fn)
    {
        return DependencyStage {
            .config = GpuComputeConfig { .shader_path = shader, .workgroup_size = { local, 1, 1 }, .push_constant_size = push_size },
            .stage_fn = std::move(fn),
            .explicit_groups = std::array<uint32_t, 3> { groups, 1U, 1U },
        };
    }

    [[nodiscard]] DependencyStage conv_stage(uint64_t src, size_t weight, std::optional<size_t> bias, uint64_t dst,
        uint32_t batch, uint32_t c_in, uint32_t t_in, uint32_t c_out, uint32_t kernel, uint32_t stride, uint32_t pad,
        uint32_t flags, uint32_t& t_out) const
    {
        t_out = (t_in + 2 * pad - kernel) / stride + 1;
        const ConvPush push {
            .src = src,
            .weights = weight_address(weight),
            .bias = bias ? weight_address(*bias) : 0,
            .dst = dst,
            .batch = batch,
            .c_in = c_in,
            .t_in = t_in,
            .c_out = c_out,
            .t_out = t_out,
            .kernel_size = kernel,
            .stride = stride,
            .pad = pad,
            .flags = flags | (bias ? k_flag_bias : 0U),
            .reserved = 0,
        };
        return stage("nn_conv1d.comp.spv", 64, sizeof(ConvPush),
            groups_for(static_cast<size_t>(batch) * c_out * t_out, 64),
            [push](GpuDispatchCore& core) { core.set_push_constants(push); });
    }

    [[nodiscard]] std::vector<float> run(std::span<const float> audio, size_t chunks)
    {
        ensure_capacity(chunks);
        const auto n = static_cast<uint32_t>(chunks);
        context->upload_shared_raw(0, Audio, reinterpret_cast<const uint8_t*>(audio.data()), audio.size_bytes());

        std::vector<DependencyStage> stages;
        const FramePush frame {
            .samples = address(Audio),
            .frames = address(Frames),
            .chunks = n,
            .chunk_size = chunk_samples,
            .context = context_samples,
            .pad = 64,
        };
        stages.push_back(stage("nn_frame_audio.comp.spv", 64, sizeof(FramePush),
            groups_for(static_cast<size_t>(n) * k_frame, 64),
            [frame](GpuDispatchCore& core) { core.set_push_constants(frame); }));

        uint32_t t = 0;
        stages.push_back(conv_stage(address(Frames), StftWeight, std::nullopt, address(Transform),
            n, 1, k_frame, 258, 256, 128, 0, 0, t));
        const uint32_t frames_out = t;

        const MagnitudePush magnitude {
            .src = address(Transform),
            .dst = address(Magnitude),
            .batch = n,
            .channels = 129,
            .time = frames_out,
            .reserved = 0,
        };
        stages.push_back(stage("nn_magnitude.comp.spv", 64, sizeof(MagnitudePush),
            groups_for(static_cast<size_t>(n) * 129 * frames_out, 64),
            [magnitude](GpuDispatchCore& core) { core.set_push_constants(magnitude); }));

        stages.push_back(conv_stage(address(Magnitude), Conv1Weight, Conv1Bias, address(Encoded1),
            n, 129, frames_out, 128, 3, 1, 1, k_relu, t));
        stages.push_back(conv_stage(address(Encoded1), Conv2Weight, Conv2Bias, address(Encoded2),
            n, 128, t, 64, 3, 2, 1, k_relu, t));
        stages.push_back(conv_stage(address(Encoded2), Conv3Weight, Conv3Bias, address(Encoded3),
            n, 64, t, 64, 3, 2, 1, k_relu, t));
        stages.push_back(conv_stage(address(Encoded3), Conv4Weight, Conv4Bias, address(Encoded4),
            n, 64, t, k_hidden, 3, 1, 1, k_relu, t));

        const LstmPush lstm {
            .x = address(Encoded4),
            .w_ih = weight_address(LstmInputWeight),
            .w_hh = weight_address(LstmHiddenWeight),
            .b_ih = weight_address(LstmInputBias),
            .b_hh = weight_address(LstmHiddenBias),
            .state = address(State),
            .hs = address(Hidden),
            .steps = n,
            .reserved = 0,
        };
        stages.push_back(stage("nn_lstm_sequence.comp.spv", 512, sizeof(LstmPush), 1,
            [lstm](GpuDispatchCore& core) { core.set_push_constants(lstm); }));

        uint32_t head_t = 0;
        stages.push_back(conv_stage(address(Hidden), HeadWeight, HeadBias, address(Probability),
            n, k_hidden, 1, 1, 1, 1, 0, k_relu_input_sigmoid(), head_t));

        context->record_stages(stages);

        std::vector<float> probabilities(chunks);
        context->download_shared(0, Probability, probabilities.data(), probabilities.size() * sizeof(float));
        return probabilities;
    }

    static constexpr uint32_t k_relu_input_sigmoid() { return k_flag_relu_input | k_sigmoid; }

    [[nodiscard]] std::vector<float> consume(size_t chunks)
    {
        std::vector<float> audio;
        audio.reserve(context_samples + chunks * chunk_samples);
        audio.insert(audio.end(), tail.begin(), tail.end());
        audio.insert(audio.end(), pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(chunks * chunk_samples));

        auto probabilities = run(audio, chunks);

        tail.assign(audio.end() - static_cast<std::ptrdiff_t>(context_samples), audio.end());
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(chunks * chunk_samples));
        return probabilities;
    }
};

std::string SileroVadModel::diagnose(const Kakshya::TensorSet& weights)
{
    for (const auto& spec : slot_specs()) {
        const auto* tensor = weights.find(spec.name);
        if (tensor == nullptr) {
            return std::format("missing tensor '{}'", spec.name);
        }
        if (tensor->spec().shape != spec.shape) {
            return std::format("tensor '{}' has shape {} but {} is required",
                spec.name, shape_text(tensor->spec().shape), shape_text(spec.shape));
        }
    }
    return {};
}

SileroVadModel::SileroVadModel(const Kakshya::TensorSet& weights)
{
    if (const std::string reason = diagnose(weights); !reason.empty()) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "SileroVadModel: {}", reason);
    }
    m_impl = std::make_unique<Impl>(weights);
}

SileroVadModel::~SileroVadModel() = default;

std::vector<float> SileroVadModel::process(std::span<const float> samples)
{
    m_impl->pending.insert(m_impl->pending.end(), samples.begin(), samples.end());

    std::vector<float> probabilities;
    for (size_t chunks = m_impl->pending.size() / chunk_samples; chunks > 0; chunks = m_impl->pending.size() / chunk_samples) {
        const auto part = m_impl->consume(std::min(chunks, k_max_chunks));
        probabilities.insert(probabilities.end(), part.begin(), part.end());
    }
    return probabilities;
}

std::vector<float> SileroVadModel::flush()
{
    if (m_impl->pending.empty()) {
        return {};
    }
    m_impl->pending.resize(chunk_samples, 0.0F);
    return m_impl->consume(1);
}

void SileroVadModel::reset()
{
    m_impl->pending.clear();
    m_impl->tail.assign(context_samples, 0.0F);
    m_impl->clear_state();
}

size_t SileroVadModel::pending() const noexcept
{
    return m_impl->pending.size();
}

} // namespace MayaFlux::Yantra
