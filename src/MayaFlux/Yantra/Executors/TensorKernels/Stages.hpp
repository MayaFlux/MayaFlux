#pragma once

#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"

#include "Layers.hpp"
#include "Layouts.hpp"

/**
 * @file Stages.hpp
 * @brief The stages that run the tensor kernels.
 *
 * Each function returns a DependencyStage for record_stages, so a network is a
 * list of stages recorded into one submission. Addresses are device addresses
 * of float32 buffers, from a context's shared buffers (shared_buffer_address).
 */

namespace MayaFlux::Yantra::TensorKernels {

namespace detail {

    [[nodiscard]] inline uint32_t groups_for(size_t invocations, uint32_t local)
    {
        return static_cast<uint32_t>((invocations + local - 1) / local);
    }

    template <typename PC>
    [[nodiscard]] DependencyStage make_stage(const char* shader, uint32_t local, const PC& pc, uint32_t groups)
    {
        return DependencyStage {
            .config = GpuComputeConfig { .shader_path = shader, .workgroup_size = { local, 1, 1 }, .push_constant_size = sizeof(PC) },
            .stage_fn = [pc](GpuDispatchCore& core) { core.set_push_constants(pc); },
            .explicit_groups = std::array<uint32_t, 3> { groups, 1U, 1U },
        };
    }

}

/**
 * @brief One nn_conv1d dispatch: [batch, c_in, t_in] to [batch, c_out, conv1d_length].
 * @param layer The layer.
 * @param src Device address of the input.
 * @param weights Device address of the weights, [c_out, c_in, kernel].
 * @param bias Device address of the bias, [c_out], or 0 for none.
 * @param dst Device address of the output.
 * @param batch Independent repeats of the input.
 * @param t_in Input length.
 * @throws std::invalid_argument When the layer is empty or the output would be empty.
 */
[[nodiscard]] inline DependencyStage conv1d_stage(const Conv1d& layer, uint64_t src, uint64_t weights,
    uint64_t bias, uint64_t dst, uint32_t batch, uint32_t t_in)
{
    const uint32_t t_out = conv1d_length(layer, t_in);
    if (layer.c_in == 0 || layer.c_out == 0 || batch == 0 || t_out == 0) {
        detail::refuse("Conv1d has no input channels, output channels, batch or output length");
    }
    const uint32_t flags = (bias != 0 ? conv1d_flag_bias : 0U)
        | (layer.relu_input ? conv1d_flag_relu_input : 0U)
        | (static_cast<uint32_t>(layer.activation) << conv1d_activation_shift);
    const Conv1dPC pc {
        .src = src,
        .weights = weights,
        .bias = bias,
        .dst = dst,
        .batch = batch,
        .c_in = layer.c_in,
        .t_in = t_in,
        .c_out = layer.c_out,
        .t_out = t_out,
        .kernel_size = layer.kernel,
        .stride = layer.stride,
        .pad = layer.pad,
        .flags = flags,
        .dilation = layer.dilation,
    };
    return detail::make_stage(conv1d_shader, element_workgroup, pc,
        detail::groups_for(static_cast<size_t>(batch) * layer.c_out * t_out, element_workgroup));
}

/**
 * @brief One nn_frame_audio dispatch: cut a sample stream into overlapping frames, reflect padded on the right.
 *
 * samples holds context + chunks * chunk_size values. Frame n is the context +
 * chunk_size values starting at n * chunk_size, followed by pad values that
 * mirror the frame about its last value without repeating it. pad must be
 * smaller than context + chunk_size.
 *
 * @param samples Device address of the samples.
 * @param frames Device address of the frames, [chunks, frame_audio_length].
 * @throws std::invalid_argument When chunks or chunk_size is 0 or pad is a frame or more.
 */
[[nodiscard]] inline DependencyStage frame_audio_stage(uint64_t samples, uint64_t frames, uint32_t chunks,
    uint32_t chunk_size, uint32_t context, uint32_t pad)
{
    if (chunks == 0 || chunk_size == 0 || pad >= context + chunk_size) {
        detail::refuse("Frame audio needs chunks, a chunk size and a pad smaller than one frame");
    }
    const FrameAudioPC pc { .samples = samples, .frames = frames, .chunks = chunks, .chunk_size = chunk_size, .context = context, .pad = pad };
    return detail::make_stage(frame_audio_shader, element_workgroup, pc,
        detail::groups_for(static_cast<size_t>(chunks) * frame_audio_length(chunk_size, context, pad), element_workgroup));
}

/**
 * @brief One nn_magnitude dispatch: sqrt(re^2 + im^2) of a complex signal stored as two channel halves.
 *
 * src is [batch, 2 * channels, time] with the real parts in the first half of
 * the channels and the imaginary parts in the second. dst is [batch, channels, time].
 *
 * @throws std::invalid_argument When batch, channels or time is 0.
 */
[[nodiscard]] inline DependencyStage magnitude_stage(uint64_t src, uint64_t dst, uint32_t batch,
    uint32_t channels, uint32_t time)
{
    if (batch == 0 || channels == 0 || time == 0) {
        detail::refuse("Magnitude needs a batch, channels and time");
    }
    const MagnitudePC pc { .src = src, .dst = dst, .batch = batch, .channels = channels, .time = time, .reserved = 0 };
    return detail::make_stage(magnitude_shader, element_workgroup, pc,
        detail::groups_for(static_cast<size_t>(batch) * channels * time, element_workgroup));
}

/**
 * @brief One nn_lstm_sequence dispatch: an LSTM cell over a whole sequence in one workgroup.
 *
 * The input width equals the hidden width. Weights are in the order of PyTorch:
 * gate rows input, forget, cell, output. w_ih and w_hh are [4 * hidden, hidden],
 * b_ih and b_hh are [4 * hidden]. state holds h then c, 2 * hidden floats, and is
 * read at the start and written at the end so the recurrence continues across
 * dispatches. hs receives the hidden vector of every step.
 *
 * @param x Device address of the input, [steps, hidden].
 * @param hs Device address of the output, [steps, hidden].
 * @param hidden Hidden width, 1 to lstm_max_hidden.
 * @throws std::invalid_argument When steps or hidden is out of range.
 */
[[nodiscard]] inline DependencyStage lstm_sequence_stage(uint64_t x, uint64_t w_ih, uint64_t w_hh, uint64_t b_ih,
    uint64_t b_hh, uint64_t state, uint64_t hs, uint32_t steps, uint32_t hidden)
{
    if (steps == 0 || hidden == 0 || hidden > lstm_max_hidden) {
        detail::refuse("LSTM needs at least one step and a hidden width from 1 to 256");
    }
    const LstmPC pc { .x = x, .w_ih = w_ih, .w_hh = w_hh, .b_ih = b_ih, .b_hh = b_hh, .state = state, .hs = hs, .steps = steps, .hidden = hidden };
    return detail::make_stage(lstm_sequence_shader, lstm_workgroup, pc, 1);
}

} // namespace MayaFlux::Yantra::TensorKernels
