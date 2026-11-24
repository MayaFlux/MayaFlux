#pragma once

#include "MayaFlux/Kakshya/FrameSource.hpp"
#include "MayaFlux/Kriya/FrameOperation.hpp"
#include "MayaFlux/Kriya/ReflowHelpers/ReflowSelect.hpp"

namespace MayaFlux::Kriya {

/**
 * @brief A selector that shows the frames of a list in proportion to weights something writes.
 * @param source The frames.
 * @param weights One weight for each frame, written by whoever steers it, such as a Nexus influence.
 *        A frame whose weight is not above zero, or that has none, is not shown.
 *
 * With several weights above zero the lane's reflow mixes the frames, or shows the heaviest one
 * without a mix. A weight vector shorter than the source weights the first frames only.
 */
template <typename Frame>
[[nodiscard]] FrameSelector<Frame> weighted_frames(
    const std::shared_ptr<const Kakshya::FrameSource<Frame>>& source,
    const std::shared_ptr<std::vector<double>>& weights)
{
    return [source, weights](const FrameContext&, std::vector<WeightedFrame<Frame>>& into) {
        if (!source || !weights) {
            return;
        }
        const size_t count = std::min(weights->size(), source->count());
        for (size_t i = 0; i < count; ++i) {
            const double weight = (*weights)[i];
            if (!(weight > 0.0) || !std::isfinite(weight)) {
                continue;
            }
            if (auto frame = source->read(i)) {
                into.push_back({ .frame = std::move(frame), .weight = weight, .index = i });
            }
        }
    };
}

/**
 * @brief A selector that shows the frames whose features are nearest a query something writes.
 * @param source The frames.
 * @param feature Describes a frame as a vector of numbers, such as a descriptor of its content.
 * @param query The vector to match, written by whoever steers it. A query of another length than
 *        the features picks nothing.
 * @param count How many nearest frames to show, weighted by the inverse of their distance.
 *
 * The features of every frame are computed the first time the selector runs, by reading every frame
 * of the source once, and kept. A frame that cannot be read or described is never picked.
 */
template <typename Frame>
[[nodiscard]] FrameSelector<Frame> nearest_by_feature(
    const std::shared_ptr<const Kakshya::FrameSource<Frame>>& source,
    std::function<std::vector<float>(const Frame&)> feature,
    const std::shared_ptr<std::vector<float>>& query, size_t count = 1)
{
    struct Table {
        bool built {};
        std::vector<std::vector<float>> features;
    };
    const auto table = std::make_shared<Table>();

    return [source, feature = std::move(feature), query, count, table](
               const FrameContext&, std::vector<WeightedFrame<Frame>>& into) {
        if (!source || !feature || !query || query->empty() || count == 0) {
            return;
        }

        if (!table->built) {
            table->built = true;
            table->features.resize(source->count());
            for (size_t i = 0; i < source->count(); ++i) {
                if (const auto frame = source->read(i)) {
                    table->features[i] = feature(*frame);
                }
            }
        }

        std::vector<std::pair<double, size_t>> ranked;
        for (size_t i = 0; i < table->features.size(); ++i) {
            const auto& described = table->features[i];
            if (described.size() != query->size()) {
                continue;
            }
            double squared = 0.0;
            for (size_t k = 0; k < described.size(); ++k) {
                const double delta = static_cast<double>(described[k]) - static_cast<double>((*query)[k]);
                squared += delta * delta;
            }
            ranked.emplace_back(std::sqrt(squared), i);
        }

        const size_t shown = std::min(count, ranked.size());
        std::ranges::partial_sort(ranked, ranked.begin() + static_cast<std::ptrdiff_t>(shown));
        for (size_t n = 0; n < shown; ++n) {
            if (auto frame = source->read(ranked[n].second)) {
                into.push_back({ .frame = std::move(frame), .weight = 1.0 / (ranked[n].first + 1.0e-6), .index = ranked[n].second });
            }
        }
    };
}

/**
 * @brief A selector whose position in the source follows a target the way a critically damped spring does.
 * @param source The frames.
 * @param target Where the position is pulled to, in the source's seconds, written by whoever steers it.
 * @param response Per second: how quickly the position closes on the target, about a third of a second
 *        at 3. Must be above zero.
 * @param selection Which of the frames around the position are shown.
 *
 * The position starts at the first target it sees and moves by the seconds of the reflow's own
 * time between cycles, so the motion is the same at any cycle rate. A target outside the source
 * shows its end.
 */
template <typename Frame>
[[nodiscard]] FrameSelector<Frame> damped_key(
    const std::shared_ptr<const Kakshya::FrameSource<Frame>>& source,
    const std::shared_ptr<double>& target, double response,
    FrameSelection selection = FrameSelection::Nearest)
{
    struct State {
        bool started {};
        double position {};
        double velocity {};
        double last {};
    };
    const auto state = std::make_shared<State>();

    return [source, target, response, selection, state](const FrameContext& context, std::vector<WeightedFrame<Frame>>& into) {
        if (!source || !target || !(response > 0.0) || !std::isfinite(*target)) {
            return;
        }

        if (!state->started) {
            state->started = true;
            state->position = *target;
            state->last = context.time;
        } else {
            const double dt = std::max(context.time - state->last, 0.0);
            state->last = context.time;
            const double stiffness = response * response;
            state->velocity += (stiffness * (*target - state->position) - 2.0 * response * state->velocity) * dt;
            state->position += state->velocity * dt;
        }

        if (const auto location = source->locate(state->position)) {
            const auto picks = Detail::select_frames(*location, selection, 1.0);
            for (size_t i = 0; i < picks.count; ++i) {
                if (auto frame = source->read(picks.items[i].index)) {
                    into.push_back({ .frame = std::move(frame), .weight = picks.items[i].weight, .index = picks.items[i].index });
                }
            }
        }
    };
}

/**
 * @brief A selector whose position in the source is integrated from a velocity that is a function of where it is and when.
 * @param source The frames.
 * @param velocity Source seconds per second at a position, in the source's seconds, and a time, in the
 *        reflow's seconds. Any callable, so a field of position or of space can steer it.
 * @param start Where the position starts, in the source's seconds.
 * @param selection Which of the frames around the position are shown.
 *
 * The position moves by velocity times the seconds between cycles, so a velocity that depends on the
 * position gives a flow that slows, speeds up or settles on its own. A velocity that is not finite
 * holds the position.
 */
template <typename Frame>
[[nodiscard]] FrameSelector<Frame> flowing_key(
    const std::shared_ptr<const Kakshya::FrameSource<Frame>>& source,
    std::function<double(double, double)> velocity, double start,
    FrameSelection selection = FrameSelection::Nearest)
{
    struct State {
        bool started {};
        double position {};
        double last {};
    };
    const auto state = std::make_shared<State>();
    state->position = start;

    return [source, velocity = std::move(velocity), selection, state](const FrameContext& context, std::vector<WeightedFrame<Frame>>& into) {
        if (!source || !velocity) {
            return;
        }

        if (!state->started) {
            state->started = true;
        } else {
            const double dt = std::max(context.time - state->last, 0.0);
            const double step = velocity(state->position, context.time) * dt;
            if (std::isfinite(step)) {
                state->position += step;
            }
        }
        state->last = context.time;

        if (const auto location = source->locate(state->position)) {
            const auto picks = Detail::select_frames(*location, selection, 1.0);
            for (size_t i = 0; i < picks.count; ++i) {
                if (auto frame = source->read(picks.items[i].index)) {
                    into.push_back({ .frame = std::move(frame), .weight = picks.items[i].weight, .index = picks.items[i].index });
                }
            }
        }
    };
}

} // namespace MayaFlux::Kriya
