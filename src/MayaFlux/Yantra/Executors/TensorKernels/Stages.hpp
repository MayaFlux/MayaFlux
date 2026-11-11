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

    /**
     * Workgroup counts covering the invocations, along x up to max_groups_per_axis and then along y. The kernels
     * that number their invocations along both axes take any such grid.
     */
    [[nodiscard]] inline std::array<uint32_t, 3> group_grid(size_t invocations, uint32_t local)
    {
        const size_t groups = std::max<size_t>((invocations + local - 1) / local, 1);
        const size_t columns = std::min<size_t>(groups, max_groups_per_axis);
        return { static_cast<uint32_t>(columns), static_cast<uint32_t>((groups + columns - 1) / columns), 1U };
    }

    template <typename PC>
    [[nodiscard]] DependencyStage make_grid_stage(const char* shader, const PC& pc, size_t invocations)
    {
        return DependencyStage {
            .config = GpuComputeConfig { .shader_path = shader, .workgroup_size = { element_workgroup, 1, 1 }, .push_constant_size = sizeof(PC) },
            .stage_fn = [pc](GpuDispatchCore& core) { core.set_push_constants(pc); },
            .explicit_groups = group_grid(invocations, element_workgroup),
        };
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
    return detail::make_grid_stage(conv1d_shader, pc, static_cast<size_t>(batch) * layer.c_out * t_out);
}

/**
 * @brief One nn_conv1d_grouped dispatch: [batch, c_in, t_in] to [batch, c_out, grouped_conv1d_length].
 * @param layer The layer. Both channel counts must be multiples of groups.
 * @param src Device address of the input.
 * @param weights Device address of the weights, [c_out, c_in / groups, kernel].
 * @param bias Device address of the bias, [c_out], or 0 for none.
 * @param dst Device address of the output.
 * @param batch Independent repeats of the input.
 * @param t_in Input length.
 * @throws std::invalid_argument When the layer is empty, the groups do not divide the channels or the output would be empty.
 */
[[nodiscard]] inline DependencyStage grouped_conv1d_stage(const GroupedConv1d& layer, uint64_t src, uint64_t weights,
    uint64_t bias, uint64_t dst, uint32_t batch, uint32_t t_in)
{
    const uint32_t t_out = grouped_conv1d_length(layer, t_in);
    if (layer.c_in == 0 || layer.c_out == 0 || batch == 0 || t_out == 0) {
        detail::refuse("Grouped Conv1d has no input channels, output channels, batch or output length");
    }
    if (layer.c_in % layer.groups != 0 || layer.c_out % layer.groups != 0) {
        detail::refuse("Grouped Conv1d needs channel counts that are multiples of its groups");
    }
    const uint32_t flags = (bias != 0 ? conv1d_flag_bias : 0U)
        | (static_cast<uint32_t>(layer.activation) << conv1d_activation_shift);
    const GroupedConv1dPC pc {
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
        .groups = layer.groups,
    };
    return detail::make_grid_stage(grouped_conv1d_shader, pc, static_cast<size_t>(batch) * layer.c_out * t_out);
}

/**
 * @brief The nn_normalize dispatch that measures every row of a tensor [outer, extent, inner] along extent.
 *
 * Writes the mean and 1 / sqrt(variance + epsilon) of each (outer, inner) row
 * to stats, two floats per row. The variance is the mean squared deviation.
 * Follow it with normalize_apply_stage.
 *
 * @param src Device address of the tensor.
 * @param stats Device address of the row measures, 2 * outer * inner floats.
 * @throws std::invalid_argument When an extent is 0.
 */
[[nodiscard]] inline DependencyStage normalize_stats_stage(uint64_t src, uint64_t stats,
    uint32_t outer, uint32_t extent, uint32_t inner, float epsilon)
{
    if (outer == 0 || extent == 0 || inner == 0) {
        detail::refuse("Normalize needs an outer, a normalised and an inner extent of at least 1");
    }
    const NormalizePC pc { .src = src, .stats = stats, .weight = 0, .bias = 0, .dst = 0,
        .outer = outer, .extent = extent, .inner = inner, .epsilon = epsilon, .flags = 0, .reserved = 0 };
    return detail::make_grid_stage(normalize_shader, pc, static_cast<size_t>(outer) * inner);
}

/**
 * @brief The nn_normalize dispatch that writes (x - mean) * scale from the measures of normalize_stats_stage.
 *
 * @param src Device address of the tensor.
 * @param stats Device address of the row measures.
 * @param weight Device address of the weight, ignored when affine is None.
 * @param bias Device address of the bias, ignored when affine is None.
 * @param dst Device address of the output, which may be src.
 * @param affine Which index selects the weight and bias.
 * @param activation Applied last. None and Gelu are supported.
 * @throws std::invalid_argument When an extent is 0 or the activation is not supported.
 */
[[nodiscard]] inline DependencyStage normalize_apply_stage(uint64_t src, uint64_t stats, uint64_t weight, uint64_t bias,
    uint64_t dst, uint32_t outer, uint32_t extent, uint32_t inner, NormAffine affine, Activation activation = Activation::None)
{
    if (outer == 0 || extent == 0 || inner == 0) {
        detail::refuse("Normalize needs an outer, a normalised and an inner extent of at least 1");
    }
    if (activation != Activation::None && activation != Activation::Gelu) {
        detail::refuse("Normalize applies no activation or GELU");
    }
    const uint32_t flags = normalize_flag_apply
        | (affine != NormAffine::None ? normalize_flag_affine : 0U)
        | (affine == NormAffine::Outer ? normalize_flag_affine_outer : 0U)
        | (static_cast<uint32_t>(activation) << normalize_activation_shift);
    const NormalizePC pc { .src = src, .stats = stats, .weight = weight, .bias = bias, .dst = dst,
        .outer = outer, .extent = extent, .inner = inner, .epsilon = 0.0F, .flags = flags, .reserved = 0 };
    return detail::make_grid_stage(normalize_shader, pc, static_cast<size_t>(outer) * extent * inner);
}

/**
 * @brief One nn_add dispatch: the element-wise sum of two sequences of count floats. dst may be a or b.
 * @throws std::invalid_argument When count is 0.
 */
[[nodiscard]] inline DependencyStage add_stage(uint64_t a, uint64_t b, uint64_t dst, uint32_t count)
{
    if (count == 0) {
        detail::refuse("Add needs at least one element");
    }
    const AddPC pc { .a = a, .b = b, .dst = dst, .count = count, .reserved = 0 };
    return detail::make_grid_stage(add_shader, pc, count);
}

/**
 * @brief One nn_attention_scores dispatch: scale times the dot product of every query with every key, per head.
 *
 * q and k are [heads * dim, time], head h owning channels h * dim onwards.
 * scores is [heads, time, time], indexed head, query, key.
 *
 * @throws std::invalid_argument When heads, dim or time is 0.
 */
[[nodiscard]] inline DependencyStage attention_scores_stage(uint64_t q, uint64_t k, uint64_t scores,
    uint32_t heads, uint32_t dim, uint32_t time, float scale)
{
    if (heads == 0 || dim == 0 || time == 0) {
        detail::refuse("Attention needs heads, a head width and positions");
    }
    const AttentionScoresPC pc { .q = q, .k = k, .scores = scores, .heads = heads, .dim = dim, .time = time, .scale = scale };
    return detail::make_grid_stage(attention_scores_shader, pc, static_cast<size_t>(heads) * time * time);
}

/**
 * @brief One nn_softmax dispatch: softmax of a tensor [outer, extent, inner] along extent, in place.
 * @throws std::invalid_argument When an extent is 0.
 */
[[nodiscard]] inline DependencyStage softmax_stage(uint64_t data, uint32_t outer, uint32_t extent, uint32_t inner)
{
    if (outer == 0 || extent == 0 || inner == 0) {
        detail::refuse("Softmax needs an outer, a normalised and an inner extent of at least 1");
    }
    const SoftmaxPC pc { .data = data, .outer = outer, .extent = extent, .inner = inner, .reserved = 0 };
    return detail::make_grid_stage(softmax_shader, pc, static_cast<size_t>(outer) * inner);
}

/**
 * @brief One nn_attention_mix dispatch: the values of every position mixed by each query's attention weights.
 *
 * weights is [heads, time, time] as attention_scores_stage and softmax_stage
 * leave it. v and dst are [heads * dim, time].
 *
 * @throws std::invalid_argument When heads, dim or time is 0.
 */
[[nodiscard]] inline DependencyStage attention_mix_stage(uint64_t weights, uint64_t v, uint64_t dst,
    uint32_t heads, uint32_t dim, uint32_t time)
{
    if (heads == 0 || dim == 0 || time == 0) {
        detail::refuse("Attention needs heads, a head width and positions");
    }
    const AttentionMixPC pc { .weights = weights, .v = v, .dst = dst, .heads = heads, .dim = dim, .time = time, .reserved = 0 };
    return detail::make_grid_stage(attention_mix_shader, pc, static_cast<size_t>(heads) * dim * time);
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
