#include "Sensor.hpp"

namespace MayaFlux::Nexus {

SensorControls& Sensor::open(QuantityStore& store)
{
    return store.declare<SensorControls>();
}

} // namespace MayaFlux::Nexus
