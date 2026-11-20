#include "SerialSource.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/InputService.hpp"
#include "MayaFlux/Vruta/NetworkSource.hpp"

namespace MayaFlux::Portal::Network {

std::shared_ptr<Vruta::NetworkSource> serial_source(uint32_t device_id, const Core::SerialSpec& framing)
{
    auto* service = Registry::BackendRegistry::instance().get_service<Registry::Service::InputService>();
    if (!service || !service->add_serial_reader || !service->remove_serial_reader) {
        MF_WARN(Journal::Component::Portal, Journal::Context::Networking,
            "serial_source: InputService not available");
        return nullptr;
    }

    Core::SerialSpec spec = framing;
    spec.parse = Core::SerialParse::BYTES;
    spec.parser = nullptr;

    auto source = std::make_shared<Vruta::NetworkSource>();
    const std::weak_ptr<Vruta::NetworkSource> weak = source;

    const uint64_t token = service->add_serial_reader(device_id, spec,
        [weak, device_id](const Core::InputValue& value) {
            const auto held = weak.lock();
            const auto* bytes = value.try_bytes();
            if (!held || !bytes) {
                return;
            }
            Core::NetworkMessage message;
            message.endpoint_id = device_id;
            message.data = *bytes;
            held->signal(message);
        });

    if (token == 0) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::Networking,
            "serial_source: serial device {} is not available", device_id);
        return nullptr;
    }

    return { source.get(), [source, token, remove = service->remove_serial_reader](Vruta::NetworkSource*) mutable {
                remove(token);
                source.reset();
            } };
}

} // namespace MayaFlux::Portal::Network
