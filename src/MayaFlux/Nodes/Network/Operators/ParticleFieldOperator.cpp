#include "ParticleFieldOperator.hpp"

namespace MayaFlux::Nodes::Network {

ParticleFieldOperator::ParticleFieldOperator(Kakshya::VertexLayout layout, ParticleFieldConfig config)
    : GpuFieldOperator(std::move(layout))
    , m_particle_config(config)
{
}

} // namespace MayaFlux::Nodes::Network
