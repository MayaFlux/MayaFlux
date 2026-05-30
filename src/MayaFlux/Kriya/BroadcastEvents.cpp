#include "BroadcastEvents.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/AudioBackendService.hpp"
#include "MayaFlux/Registry/Service/DisplayService.hpp"

namespace MayaFlux::Kriya {

std::shared_ptr<Vruta::BroadcastSource<bool>> audio_output_tick()
{
    auto* svc = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::AudioBackendService>();

    if (!svc)
        return nullptr;

    struct State {
        Vruta::BroadcastSource<bool> source;
        Registry::Service::AudioBackendService* svc { nullptr };
        uint32_t observer_id { 0 };
        ~State()
        {
            if (svc)
                svc->unregister_output_observer(observer_id);
        }
    };

    auto state = std::make_shared<State>();
    state->svc = svc;
    state->observer_id = svc->register_output_observer(
        [s = state.get()](const double*, uint32_t) {
            s->source.signal(true);
        });

    return { state, &state->source };
}

std::shared_ptr<Vruta::BroadcastSource<WindowFrame>> window_frame_tick(
    const std::shared_ptr<Core::Window>& window)
{
    auto* svc = Registry::BackendRegistry::instance()
                    .get_service<Registry::Service::DisplayService>();

    if (!svc || !window)
        return nullptr;

    struct State {
        Vruta::BroadcastSource<WindowFrame> source;
        Registry::Service::DisplayService* svc { nullptr };
        std::shared_ptr<Core::Window> window;
        uint32_t observer_id { 0 };

        ~State()
        {
            if (svc && observer_id) {
                svc->unregister_frame_observer(
                    std::static_pointer_cast<void>(window), observer_id);
            }
        }
    };

    auto state = std::make_shared<State>();
    state->svc = svc;
    state->window = window;

    state->observer_id = svc->register_frame_observer(
        std::static_pointer_cast<void>(window),

        [weak = std::weak_ptr<State>(state)](
            const std::shared_ptr<std::vector<uint8_t>>& buf,
            uint32_t w, uint32_t h, uint32_t fmt) {
            if (!buf) {
                return;
            }

            if (auto s = weak.lock())
                s->source.signal({ .data = buf, .width = w, .height = h, .format = fmt });
        });

    if (!state->observer_id)
        return nullptr;

    return { state, &state->source };
}

} // namespace MayaFlux::Kriya
