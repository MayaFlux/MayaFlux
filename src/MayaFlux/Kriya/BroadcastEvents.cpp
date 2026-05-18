#include "BroadcastEvents.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/AudioBackendService.hpp"

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

} // namespace MayaFlux::Kriya
