#include "SDFFieldProcessor.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {

    /**
     * @brief Build the compute shader that evaluates @p field at every grid corner.
     *
     * Every push constant is a 4-byte scalar, so the block has no padding and
     * matches the host struct byte for byte.
     */
    [[nodiscard]] Portal::Graphics::ShaderSpec field_spec(const Kinesis::FieldSource& field)
    {
        if (!field.valid() || field.parameter_count() > 3) {
            error<std::invalid_argument>(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "SDFFieldProcessor: the field did not parse into a function of "
                "one to three parameters. It needs a trailing return type and a body.");
        }

        using Kakshya::GpuDataFormat;

        Portal::Graphics::ShaderSpec::Assemble assemble;
        assemble.ssbo("sdf_grid", Portal::Graphics::BindingDirection::Output, GpuDataFormat::FLOAT32);

        for (const char* name : { "bounds_min_x", "bounds_min_y", "bounds_min_z", "time", "step_x", "step_y", "step_z" }) {
            assemble.pc(name);
        }
        for (const char* name : { "res_x", "res_y", "res_z" }) {
            assemble.pc(name, GpuDataFormat::UINT32);
        }
        for (const char* name : { "p0", "p1", "p2", "p3" }) {
            assemble.pc(name);
        }

        assemble.workgroup(64)
            .function(field.return_type, field.name, field.params, field.body);

        std::string body;
        body += "    uint nx = res_x + 1u;\n";
        body += "    uint ny = res_y + 1u;\n";
        body += "    if (i >= nx * ny * (res_z + 1u)) { return; }\n";
        body += "    uint ix = i % nx;\n";
        body += "    uint iy = (i / nx) % ny;\n";
        body += "    uint iz = i / (nx * ny);\n";
        body += "    vec3 p = vec3(bounds_min_x + float(ix) * step_x,"
                " bounds_min_y + float(iy) * step_y,"
                " bounds_min_z + float(iz) * step_z);\n";

        switch (field.parameter_count()) {
        case 1:
            body += "    sdf_grid[i] = " + field.name + "(p);\n";
            break;
        case 2:
            body += "    sdf_grid[i] = " + field.name + "(p, time);\n";
            break;
        default:
            body += "    vec4 params = vec4(p0, p1, p2, p3);\n";
            body += "    sdf_grid[i] = " + field.name + "(p, time, params);\n";
            break;
        }

        assemble.kernel(Portal::Graphics::KernelSource { .body = std::move(body) });
        return assemble.build();
    }

} // namespace

SDFFieldProcessor::SDFFieldProcessor(
    std::shared_ptr<VKBuffer> grid_buf,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z,
    std::string shader)
    : ComputeProcessor(shader, 64)
    , m_grid_buf(std::move(grid_buf))
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
{
    m_pc.bounds_min = bounds_min;
    m_pc.res_x = m_res_x;
    m_pc.res_y = m_res_y;
    m_pc.res_z = m_res_z;

    const glm::vec3 extent = bounds_max - bounds_min;
    m_pc.step = {
        extent.x / static_cast<float>(m_res_x),
        extent.y / static_cast<float>(m_res_y),
        extent.z / static_cast<float>(m_res_z),
    };

    m_config.push_constant_size = sizeof(PC);
    m_config.bindings["sdf_grid"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);

    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
    const uint32_t corners = (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    set_manual_dispatch((corners + 63U) / 64U, 1, 1);
}

SDFFieldProcessor::SDFFieldProcessor(
    std::shared_ptr<VKBuffer> grid_buf,
    const Kinesis::FieldSource& field,
    const glm::vec3& bounds_min,
    const glm::vec3& bounds_max,
    uint32_t res_x,
    uint32_t res_y,
    uint32_t res_z)
    : ComputeProcessor(field_spec(field))
    , m_from_field(true)
    , m_grid_buf(std::move(grid_buf))
    , m_res_x(std::max(res_x, 1U))
    , m_res_y(std::max(res_y, 1U))
    , m_res_z(std::max(res_z, 1U))
{
    m_field_pc.res_x = m_res_x;
    m_field_pc.res_y = m_res_y;
    m_field_pc.res_z = m_res_z;
    set_bounds(bounds_min, bounds_max);

    m_config.bindings["sdf_grid"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);

    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
    const uint32_t corners = (m_res_x + 1) * (m_res_y + 1) * (m_res_z + 1);
    set_manual_dispatch((corners + 63U) / 64U, 1, 1);
}

void SDFFieldProcessor::set_bounds(const glm::vec3& bounds_min, const glm::vec3& bounds_max)
{
    m_pc.bounds_min = bounds_min;
    const glm::vec3 extent = bounds_max - bounds_min;
    m_pc.step = {
        extent.x / static_cast<float>(m_res_x),
        extent.y / static_cast<float>(m_res_y),
        extent.z / static_cast<float>(m_res_z),
    };

    m_field_pc.bounds_min[0] = bounds_min.x;
    m_field_pc.bounds_min[1] = bounds_min.y;
    m_field_pc.bounds_min[2] = bounds_min.z;
    m_field_pc.step[0] = m_pc.step.x;
    m_field_pc.step[1] = m_pc.step.y;
    m_field_pc.step[2] = m_pc.step.z;
}

void SDFFieldProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);
    bind_buffer("sdf_grid", m_grid_buf);
}

bool SDFFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& /*buffer*/)
{
    if (m_from_field) {
        set_push_constant_data(m_field_pc);
    } else {
        set_push_constant_data(m_pc);
    }
    return true;
}

} // namespace MayaFlux::Buffers
