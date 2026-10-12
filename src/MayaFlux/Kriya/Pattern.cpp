#include "Pattern.hpp"

namespace MayaFlux::Kriya {

std::shared_ptr<Vruta::Routine> pattern(
    std::function<std::any(uint64_t)> pattern_func,
    std::function<void(std::any)> callback,
    std::function<bool(uint64_t)> ready,
    Vruta::ProcessingToken token)
{
    return pattern<uint64_t>(std::move(pattern_func), std::move(callback), std::move(ready), token);
}

} // namespace MayaFlux::Kriya
