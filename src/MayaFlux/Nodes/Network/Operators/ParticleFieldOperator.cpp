#include "ParticleFieldOperator.hpp"

namespace MayaFlux::Nodes::Network {

ParticleFieldOperator::ParticleFieldOperator(Kakshya::VertexLayout layout, ParticleFieldConfig config)
    : GpuFieldOperator(std::move(layout))
    , m_particle_config(config)
{
}

void ParticleFieldOperator::set_density_saturation_count(float count)
{
    m_particle_config.density_saturation_count = count;
    invalidate();
}

void ParticleFieldOperator::set_capture_growth(float growth)
{
    m_particle_config.capture_growth = growth;
    invalidate();
}

void ParticleFieldOperator::set_swallow_base_size(float size)
{
    m_particle_config.swallow_base_size = size;
    invalidate();
}

void ParticleFieldOperator::set_swallow_growth_rate(float rate)
{
    m_particle_config.swallow_growth_rate = rate;
    invalidate();
}

void ParticleFieldOperator::set_swallow_max_size(float size)
{
    m_particle_config.swallow_max_size = size;
    invalidate();
}

void ParticleFieldOperator::set_swallow_dim_factor(float factor)
{
    m_particle_config.swallow_dim_factor = factor;
    invalidate();
}

void ParticleFieldOperator::set_cross_cluster(bool enabled)
{
    m_particle_config.cross_cluster = enabled;
    invalidate();
}

} // namespace MayaFlux::Nodes::Network
