#include "BuoyancyProcessor.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr size_t k_tail_offset = 32;
    constexpr size_t k_param_size = 64;

    /**
     * @struct BuoyancyTail
     * @brief The parameter words past the shared lattice prefix.
     */
    struct BuoyancyTail {
        float direction_x;
        float direction_y;
        float direction_z;
        float ambient;
        float temperature_gain;
        float density_gain;
        float pad1;
        float pad2;
    };

    static_assert(k_tail_offset + sizeof(BuoyancyTail) == k_param_size);
}

std::vector<VolumeFieldProcessor::FieldBinding> BuoyancyProcessor::make_bindings(
    const std::string& temperature_field,
    const std::string& density_field,
    const std::string& velocity_field)
{
    return {
        { .name = "temperature_in", .binding = 0, .field = temperature_field, .access = FieldAccess::READ },
        { .name = "density_in", .binding = 1, .field = density_field, .access = FieldAccess::READ },
        { .name = "velocity_in", .binding = 2, .field = velocity_field, .access = FieldAccess::READ },
        { .name = "velocity_out", .binding = 3, .field = velocity_field, .access = FieldAccess::WRITE },
    };
}

BuoyancyProcessor::BuoyancyProcessor(
    std::string temperature_field,
    std::string density_field,
    std::string velocity_field,
    const glm::vec3& direction,
    const std::string& shader_path)
    : VolumeFieldProcessor(
          make_bindings(temperature_field, density_field, velocity_field), shader_path)
    , m_temperature_field(std::move(temperature_field))
    , m_density_field(std::move(density_field))
    , m_velocity_field(std::move(velocity_field))
    , m_direction(direction)
{
    add_swap_field(m_velocity_field);
}

BuoyancyProcessor::BuoyancyProcessor(
    std::string temperature_field,
    std::string density_field,
    std::string velocity_field,
    const glm::vec3& direction,
    const Portal::Graphics::ShaderSpec& spec)
    : VolumeFieldProcessor(
          make_bindings(temperature_field, density_field, velocity_field), spec)
    , m_temperature_field(std::move(temperature_field))
    , m_density_field(std::move(density_field))
    , m_velocity_field(std::move(velocity_field))
    , m_direction(direction)
{
    add_swap_field(m_velocity_field);
}

void BuoyancyProcessor::on_volume_ready()
{
    reserve_param_size(k_param_size);
    write_lattice_params();
    write_tail();
}

void BuoyancyProcessor::write_tail()
{
    write_lattice_word7(m_time_step);

    const BuoyancyTail tail {
        .direction_x = m_direction.x,
        .direction_y = m_direction.y,
        .direction_z = m_direction.z,
        .ambient = m_ambient,
        .temperature_gain = m_temperature_gain,
        .density_gain = m_density_gain,
        .pad1 = 0.0F,
        .pad2 = 0.0F,
    };

    write_param_tail(k_tail_offset, &tail, sizeof(tail));
}

void BuoyancyProcessor::set_time_step(float dt)
{
    m_time_step = dt;
    if (get_volume()) {
        write_tail();
    }
}

void BuoyancyProcessor::set_direction(const glm::vec3& direction)
{
    m_direction = direction;
    if (get_volume()) {
        write_tail();
    }
}

void BuoyancyProcessor::set_ambient(float ambient)
{
    m_ambient = ambient;
    if (get_volume()) {
        write_tail();
    }
}

void BuoyancyProcessor::set_temperature_gain(float gain)
{
    m_temperature_gain = gain;
    if (get_volume()) {
        write_tail();
    }
}

void BuoyancyProcessor::set_density_gain(float gain)
{
    m_density_gain = gain;
    if (get_volume()) {
        write_tail();
    }
}

} // namespace MayaFlux::Buffers
