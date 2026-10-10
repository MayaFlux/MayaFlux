#include "Pattern.hpp"

#include "Awaiters/DelayAwaiters.hpp"
#include "MayaFlux/Vruta/ChronUtils.hpp"

namespace MayaFlux::Kriya {

std::shared_ptr<Vruta::Routine> pattern(std::function<std::any(uint64_t)> pattern_func, std::function<void(std::any)> callback, double interval_seconds, Vruta::ProcessingToken token)
{
    if (token == Vruta::ProcessingToken::FRAME_ACCURATE) {
        auto coro = [](std::function<std::any(uint64_t)> fn, std::function<void(std::any)> cb, double interval) -> Vruta::GraphicsRoutine {
            uint64_t units = Vruta::seconds_to_frames(interval);
            uint64_t step = 0;
            while (true) {
                cb(fn(step++));
                co_await FrameDelay { .frames_to_wait = units };
            }
        };
        return std::make_shared<Vruta::GraphicsRoutine>(coro(std::move(pattern_func), std::move(callback), interval_seconds));
    }

    auto coro = [](std::function<std::any(uint64_t)> fn, std::function<void(std::any)> cb, double interval) -> Vruta::SoundRoutine {
        uint64_t units = Vruta::seconds_to_samples(interval);
        uint64_t step = 0;
        while (true) {
            cb(fn(step++));
            co_await SampleDelay { units };
        }
    };
    return std::make_shared<Vruta::SoundRoutine>(coro(std::move(pattern_func), std::move(callback), interval_seconds));
}

std::shared_ptr<Vruta::Routine> pattern(
    std::function<std::any(uint64_t)> pattern_func,
    std::function<void(std::any)> callback,
    std::function<bool(uint64_t)> ready,
    Vruta::ProcessingToken token)
{
    return pattern<uint64_t>(std::move(pattern_func), std::move(callback), std::move(ready), token);
}

} // namespace MayaFlux::Kriya
