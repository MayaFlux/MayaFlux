#include "ExecutionContext.hpp"

#include "MayaFlux/Yantra/Executors/GpuDispatchCore.hpp"

namespace MayaFlux::Yantra {

DependencyParams::DependencyParams() = default;
DependencyParams::~DependencyParams() = default;
DependencyParams::DependencyParams(const DependencyParams&) = default;
DependencyParams::DependencyParams(DependencyParams&&) noexcept = default;
DependencyParams& DependencyParams::operator=(const DependencyParams&) = default;
DependencyParams& DependencyParams::operator=(DependencyParams&&) noexcept = default;

}
