#include "InfluxProcessor.hpp"

#include "VolumeGridBuffer.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr size_t k_tail_offset = 32;
    constexpr size_t k_param_size = 80;

    /**
     * @struct InfluxTail
     * @brief The parameter words past the shared lattice prefix.
     */
    struct InfluxTail {
        float center_x;
        float center_y;
        float center_z;
        float radius;
        float rate;
        float time_step;
        float falloff;
        float pad1;
        float bounds_min_x;
        float bounds_min_y;
        float bounds_min_z;
        float pad2;
    };

    static_assert(k_tail_offset + sizeof(InfluxTail) == k_param_size);
}

std::vector<VolumeFieldProcessor::FieldBinding> InfluxProcessor::make_bindings(
    const std::string& field)
{
    return {
        { .name = "field_in", .binding = 0, .field = field, .access = FieldAccess::READ },
        { .name = "field_out", .binding = 1, .field = field, .access = FieldAccess::WRITE },
    };
}

InfluxProcessor::InfluxProcessor(std::string field, const std::string& shader_path)
    : VolumeFieldProcessor(make_bindings(field), shader_path)
    , m_field(std::move(field))
{
    add_swap_field(m_field);
}

InfluxProcessor::InfluxProcessor(std::string field, const Portal::Graphics::ShaderSpec& spec)
    : VolumeFieldProcessor(make_bindings(field), spec)
    , m_field(std::move(field))
{
    add_swap_field(m_field);
}

void InfluxProcessor::on_volume_ready()
{
    reserve_param_size(k_param_size);
    write_lattice_params();
    write_tail();
}

void InfluxProcessor::write_tail()
{
    write_lattice_word7(m_elapsed);

    const glm::vec3 lo = get_volume()
        ? get_volume()->get_lattice().bounds.min
        : glm::vec3(0.0F);

    const InfluxTail tail {
        .center_x = m_center.x,
        .center_y = m_center.y,
        .center_z = m_center.z,
        .radius = m_radius,
        .rate = m_rate,
        .time_step = m_time_step,
        .falloff = m_falloff,
        .pad1 = 0.0F,
        .bounds_min_x = lo.x,
        .bounds_min_y = lo.y,
        .bounds_min_z = lo.z,
        .pad2 = 0.0F,
    };

    write_param_tail(k_tail_offset, &tail, sizeof(tail));
}

void InfluxProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (get_volume()) {
        m_elapsed += m_time_step;
        write_lattice_word7(m_elapsed);
    }

    VolumeFieldProcessor::processing_function(buffer);
}

void InfluxProcessor::restart()
{
    m_elapsed = 0.0F;
    if (get_volume()) {
        write_lattice_word7(m_elapsed);
    }
}

void InfluxProcessor::set_center(const glm::vec3& center)
{
    m_center = center;
    if (get_volume()) {
        write_tail();
    }
}

void InfluxProcessor::set_radius(float radius)
{
    m_radius = radius;
    if (get_volume()) {
        write_tail();
    }
}

void InfluxProcessor::set_rate(float rate)
{
    m_rate = rate;
    if (get_volume()) {
        write_tail();
    }
}

void InfluxProcessor::set_time_step(float dt)
{
    m_time_step = dt;
    if (get_volume()) {
        write_tail();
    }
}

void InfluxProcessor::set_falloff(float falloff)
{
    m_falloff = falloff;
    if (get_volume()) {
        write_tail();
    }
}

} // namespace MayaFlux::Buffers
