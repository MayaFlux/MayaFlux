#pragma once
#define MAYASIMPLE

#include "MayaFlux/MayaFlux.hpp"

namespace MayaFlux {

/**
 * @brief JIT-compatible wrappers for MayaFlux API functions.
 *
 * LLVM < 21 limitation: In-place lambdas cause infinite recursion during symbol resolution.
 * Workaround: All global MayaFlux API functions that expect std::function have template
 * wrappers here that accept callables directly. Use these in JIT code.
 *
 * Example (JIT):
 *   schedule_metro(0.5, [](){ std::cout << "tick"; }, "my_metro");  // ✓ Works
 *
 * Once LLVM 21+ is standard, in-place lambdas will work directly without these wrappers.
 *
 * @note This header is automatically included in the PCH for JIT contexts (MAYASIMPLE mode).
 */

#ifdef LILA_WORKAROUND

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

/**
 * @brief JIT wrapper for Gate coroutine
 * @param scheduler Task scheduler instance
 * @param callback Function to execute while gate is open
 * @param logic_node Logic node to control gate (creates default threshold node if null)
 * @param open Initial gate state (true = open, false = closed)
 * @return SoundRoutine coroutine handle
 */
template <typename Callable>
Vruta::SoundRoutine Gate(
    Vruta::TaskScheduler& scheduler, Callable&& callback,
    std::shared_ptr<Nodes::Generator::Logic> logic_node, bool open = true)
{
    std::function<void()> func = std::forward<Callable>(callback);
    return Gate(scheduler, std::move(func), std::move(logic_node), open);
}

/**
 * @brief JIT wrapper for Trigger coroutine
 * @param scheduler Task scheduler instance
 * @param target_state State to trigger on (true/false)
 * @param callback Function to execute on state change
 * @param logic_node Logic node to monitor (creates default threshold node if null)
 * @return SoundRoutine coroutine handle
 */
template <typename Callable>
Vruta::SoundRoutine Trigger(
    Vruta::TaskScheduler& scheduler,
    bool target_state,
    Callable&& callback,
    std::shared_ptr<Nodes::Generator::Logic> logic_node)
{
    std::function<void()> func = std::forward<Callable>(callback);
    return Trigger(scheduler, target_state, std::move(func), std::move(logic_node));
}

/**
 * @brief JIT wrapper for Toggle coroutine
 * @param scheduler Task scheduler instance
 * @param callback Function to execute on any state flip
 * @param logic_node Logic node to monitor (creates default threshold node if null)
 * @return SoundRoutine coroutine handle
 */
template <typename Callable>
Vruta::SoundRoutine Toggle(
    Vruta::TaskScheduler& scheduler,
    Callable&& callback,
    std::shared_ptr<Nodes::Generator::Logic> logic_node)
{
    std::function<void()> func = std::forward<Callable>(callback);
    return Toggle(scheduler, std::move(func), std::move(logic_node));
}

#endif // LILA_WORKAROUND

}
