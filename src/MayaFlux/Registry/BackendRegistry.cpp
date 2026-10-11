#include "BackendRegistry.hpp"

namespace MayaFlux::Registry {

BackendRegistry& BackendRegistry::instance()
{
    static BackendRegistry registry;
    return registry;
}

}
