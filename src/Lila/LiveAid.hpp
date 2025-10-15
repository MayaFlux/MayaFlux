#pragma once
#define MAYASIMPLE

#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux {

template <typename Callable>
void schedule_metro(double interval_seconds, Callable&& callback, std::string name = "")
{
    std::function<void()> func = std::forward<Callable>(callback);
    schedule_metro(interval_seconds, std::move(func), std::move(name));
}

template <typename... Args>
void schedule_sequence(const std::vector<std::pair<double, std::function<void(Args...)>>>& sequence,
    std::string name = "", Args... args)
{
    std::vector<std::pair<double, std::function<void()>>> seq;
    for (const auto& [time, func] : sequence) {
        seq.emplace_back(time, [func, args...]() { func(args...); });
    }
    schedule_sequence(std::move(seq), std::move(name));
}

template <typename PatternFunc, typename Callback>
void schedule_pattern(PatternFunc&& pattern_func, Callback&& callback,
    double interval_seconds, std::string name = "")
{
    /*
        std::function<std::any(uint64_t)> pf = std::forward<PatternFunc>(pattern_func);
        std::function<void(std::any)> cb = std::forward<Callback>(callback);
        schedule_pattern(std::move(pf), std::move(cb), interval_seconds, std::move(name));
    */
    auto pattern_wrapper = [pf = std::forward<PatternFunc>(pattern_func)](uint64_t idx) -> std::any {
        return pf(idx);
    };

    auto callback_wrapper = [cb = std::forward<Callback>(callback)](std::any val) {
        cb(val);
    };

    std::function<std::any(uint64_t)> pattern_fn = pattern_wrapper;
    std::function<void(std::any)> callback_fn = callback_wrapper;

    schedule_pattern(std::move(pattern_fn), std::move(callback_fn),
        interval_seconds, std::move(name));
}

template <typename FuncType>
std::shared_ptr<Buffers::BufferProcessor> attach_quick_process(FuncType&& processor, std::shared_ptr<Buffers::AudioBuffer> buffer)
{
    std::function<void(std::shared_ptr<Buffers::AudioBuffer>)> func = std::forward<FuncType>(processor);
    return attach_quick_process(std::move(func), std::move(buffer));
}

}
