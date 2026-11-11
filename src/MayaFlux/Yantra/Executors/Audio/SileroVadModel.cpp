#include "SileroVadModel.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"
#include "MayaFlux/Yantra/Executors/TensorKernels/Stages.hpp"
#include "MayaFlux/Yantra/Executors/TensorKernels/Weights.hpp"
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

    using TensorKernels::TensorRequirement;

    const std::array<TensorRequirement, SlotCount>& slot_specs()
    {
        static const std::array<TensorRequirement, SlotCount> specs { {
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

    constexpr uint32_t k_reflect_pad = 64;
    constexpr uint32_t k_frame = SileroVadModel::context_samples + SileroVadModel::chunk_samples + k_reflect_pad;
    constexpr uint32_t k_hidden = 128;
    constexpr size_t k_max_chunks = 4096;

}

struct SileroVadModel::Impl {
    std::shared_ptr<GpuDispatchRecorder> recording;
    std::unique_ptr<ShaderExecutionContext<>> context;
    TensorKernels::PackedWeights layout;
    size_t capacity {};
    std::vector<float> pending;
    std::vector<float> tail;
    size_t resident { 0 };
    bool keep_hidden { false };
    std::vector<float> kept_hidden;

    Impl(TensorKernels::PackedWeights packed, std::shared_ptr<GpuDispatchRecorder> shared)
        : recording(shared ? std::move(shared) : std::make_shared<GpuDispatchRecorder>(true))
        , context(std::make_unique<ShaderExecutionContext<>>(
              GpuComputeConfig { .shader_path = TensorKernels::frame_audio_shader, .workgroup_size = { TensorKernels::element_workgroup, 1, 1 }, .push_constant_size = sizeof(TensorKernels::FrameAudioPC) }))
        , tail(context_samples, 0.0F)
    {
        context->set_recording(recording);
        if (!context->ensure_gpu_ready()) {
            error<std::runtime_error>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
                std::source_location::current(), "SileroVadModel needs a graphics device and none is available");
        }
        TensorKernels::upload_packed_weights(*context, 0, Weights, packed);
        packed.values = std::vector<float> {};
        layout = std::move(packed);

        context->ensure_shared_buffer(0, State, 2 * k_hidden, GpuBufferBinding::ElementType::FLOAT32, Portal::Graphics::BufferUsageHint::COMPUTE);
        clear_state();
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
        return layout.address(address(Weights), slot);
    }

    [[nodiscard]] std::vector<float> run(std::span<const float> audio, size_t chunks, bool read = true)
    {
        ensure_capacity(chunks);
        const auto n = static_cast<uint32_t>(chunks);
        context->upload_shared_raw(0, Audio, reinterpret_cast<const uint8_t*>(audio.data()), audio.size_bytes());

        std::vector<DependencyStage> stages;
        stages.push_back(TensorKernels::frame_audio_stage(address(Audio), address(Frames), n, chunk_samples, context_samples, k_reflect_pad));

        uint32_t t = k_frame;
        const auto convolve = [&](const TensorKernels::Conv1d& layer, uint64_t src, size_t weight, std::optional<size_t> bias, uint64_t dst) {
            stages.push_back(TensorKernels::conv1d_stage(layer, src, weight_address(weight), bias ? weight_address(*bias) : 0, dst, n, t));
            t = TensorKernels::conv1d_length(layer, t);
        };

        convolve({ .c_in = 1, .c_out = 258, .kernel = 256, .stride = 128 }, address(Frames), StftWeight, std::nullopt, address(Transform));
        stages.push_back(TensorKernels::magnitude_stage(address(Transform), address(Magnitude), n, 129, t));

        constexpr auto relu = TensorKernels::Activation::Relu;
        convolve({ .c_in = 129, .c_out = 128, .kernel = 3, .pad = 1, .activation = relu }, address(Magnitude), Conv1Weight, Conv1Bias, address(Encoded1));
        convolve({ .c_in = 128, .c_out = 64, .kernel = 3, .stride = 2, .pad = 1, .activation = relu }, address(Encoded1), Conv2Weight, Conv2Bias, address(Encoded2));
        convolve({ .c_in = 64, .c_out = 64, .kernel = 3, .stride = 2, .pad = 1, .activation = relu }, address(Encoded2), Conv3Weight, Conv3Bias, address(Encoded3));
        convolve({ .c_in = 64, .c_out = k_hidden, .kernel = 3, .pad = 1, .activation = relu }, address(Encoded3), Conv4Weight, Conv4Bias, address(Encoded4));

        stages.push_back(TensorKernels::lstm_sequence_stage(address(Encoded4), weight_address(LstmInputWeight), weight_address(LstmHiddenWeight),
            weight_address(LstmInputBias), weight_address(LstmHiddenBias), address(State), address(Hidden), n, k_hidden));

        t = 1;
        convolve({ .c_in = k_hidden, .c_out = 1, .kernel = 1, .relu_input = true, .activation = TensorKernels::Activation::Sigmoid },
            address(Hidden), HeadWeight, HeadBias, address(Probability));

        context->record_stages(stages);
        resident = chunks;
        if (!read) {
            return {};
        }

        std::vector<float> probabilities(chunks);
        context->download_shared(0, Probability, probabilities.data(), probabilities.size() * sizeof(float));
        if (keep_hidden) {
            const size_t held = kept_hidden.size();
            kept_hidden.resize(held + chunks * k_hidden);
            context->download_shared(0, Hidden, kept_hidden.data() + held, chunks * k_hidden * sizeof(float));
        }
        return probabilities;
    }

    [[nodiscard]] std::vector<float> consume(size_t chunks, bool read = true)
    {
        std::vector<float> audio;
        audio.reserve(context_samples + chunks * chunk_samples);
        audio.insert(audio.end(), tail.begin(), tail.end());
        audio.insert(audio.end(), pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(chunks * chunk_samples));

        auto probabilities = run(audio, chunks, read);

        tail.assign(audio.end() - static_cast<std::ptrdiff_t>(context_samples), audio.end());
        pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(chunks * chunk_samples));
        return probabilities;
    }
};

std::string SileroVadModel::diagnose(const Kakshya::TensorSet& weights)
{
    return TensorKernels::check_tensors(weights, slot_specs());
}

StreamFraming SileroVadModel::framing() noexcept
{
    return { sample_rate, chunk_samples, context_samples, hidden_width };
}

SileroVadModel::SileroVadModel(const Kakshya::TensorSet& weights)
    : SileroVadModel(weights, nullptr)
{
}

SileroVadModel::SileroVadModel(const Kakshya::TensorSet& weights, std::shared_ptr<GpuDispatchRecorder> recording)
{
    auto packed = TensorKernels::pack_weights(weights, slot_specs());
    if (!packed) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "SileroVadModel: {}", packed.error());
    }
    m_impl = std::make_unique<Impl>(std::move(*packed), std::move(recording));
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

SileroVadModel::ProbabilityBuffer SileroVadModel::probability_buffer()
{
    m_impl->ensure_capacity(std::max<size_t>(m_impl->capacity, 1));
    return { .context = m_impl->context.get(), .set = 0, .binding = Probability };
}

SileroVadModel::ProbabilityBuffer SileroVadModel::hidden_buffer()
{
    m_impl->ensure_capacity(std::max<size_t>(m_impl->capacity, 1));
    return { .context = m_impl->context.get(), .set = 0, .binding = Hidden };
}

void SileroVadModel::keep_hidden(bool keep)
{
    m_impl->keep_hidden = keep;
    if (!keep) {
        m_impl->kept_hidden.clear();
    }
}

std::vector<float> SileroVadModel::take_hidden()
{
    return std::exchange(m_impl->kept_hidden, {});
}

size_t SileroVadModel::record(std::span<const float> samples)
{
    m_impl->pending.insert(m_impl->pending.end(), samples.begin(), samples.end());

    const size_t chunks = std::min(m_impl->pending.size() / chunk_samples, k_max_chunks);
    if (chunks == 0) {
        return 0;
    }
    static_cast<void>(m_impl->consume(chunks, false));
    return chunks;
}

size_t SileroVadModel::resident_count() const noexcept
{
    return m_impl->resident;
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
    m_impl->resident = 0;
    m_impl->kept_hidden.clear();
}

size_t SileroVadModel::pending() const noexcept
{
    return m_impl->pending.size();
}

} // namespace MayaFlux::Yantra
