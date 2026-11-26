#include "Sensor.hpp"

namespace MayaFlux::Nexus {

PlacementControls& Sensor::open_placement(QuantityStore& store)
{
    return store.declare<PlacementControls>();
}

SensorControls& Sensor::open(QuantityStore& store)
{
    return store.extend<SensorControls>();
}

} // namespace MayaFlux::Nexus
