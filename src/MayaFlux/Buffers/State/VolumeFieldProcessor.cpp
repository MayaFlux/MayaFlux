#include "VolumeFieldProcessor.hpp"
#include "VolumeGridBuffer.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr uint32_t k_workgroup_x = 8;
    constexpr uint32_t k_workgroup_y = 8;
    constexpr uint32_t k_workgroup_z = 4;
    constexpr size_t k_word3_offset = offsetof(VolumeFieldProcessor::LatticeParams, word3);
    constexpr size_t k_word7_offset = offsetof(VolumeFieldProcessor::LatticeParams, word7);
}

VolumeFieldProcessor::VolumeFieldProcessor(
    std::vector<FieldBinding> bindings, const std::string& shader_path)
    : ComputeProcessor(shader_path, k_workgroup_x)
    , m_bindings(std::move(bindings))
{
    register_bindings();
}

VolumeFieldProcessor::VolumeFieldProcessor(
    std::vector<FieldBinding> bindings, const Portal::Graphics::ShaderSpec& spec)
    : ComputeProcessor(spec)
    , m_bindings(std::move(bindings))
{
    register_bindings();
}

void VolumeFieldProcessor::register_bindings()
{
    for (const auto& entry : m_bindings) {
        m_config.bindings[entry.name]
            = ShaderBinding(0, entry.binding, vk::DescriptorType::eStorageBuffer);
    }
}

void VolumeFieldProcessor::add_swap_field(std::string field)
{
    m_swap_fields.push_back(std::move(field));
}

void VolumeFieldProcessor::reserve_param_size(size_t size)
{
    if (m_config.push_constant_size < size) {
        set_push_constant_size(size);
    }
}

bool VolumeFieldProcessor::validate_fields()
{
    for (const auto& entry : m_bindings) {
        if (!m_volume->has_field(entry.field)) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "VolumeFieldProcessor: volume has no field '{}' for binding '{}'",
                entry.field, entry.name);
            return false;
        }
    }

    for (const auto& entry : m_bindings) {
        if (entry.access != FieldAccess::WRITE) {
            continue;
        }

        const bool also_read = std::ranges::any_of(m_bindings, [&](const FieldBinding& other) {
            return other.access == FieldAccess::READ && other.field == entry.field;
        });

        if (!also_read) {
            continue;
        }

        if (m_volume->read_handle(entry.field) == m_volume->write_handle(entry.field)) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "VolumeFieldProcessor: field '{}' is bound for both read and write "
                "but is not double-buffered",
                entry.field);
            return false;
        }
    }

    return true;
}

void VolumeFieldProcessor::size_dispatch()
{
    set_workgroup_size(k_workgroup_x, k_workgroup_y, k_workgroup_z);
    set_dispatch_mode(ShaderDispatchConfig::DispatchMode::MANUAL);
    set_manual_dispatch(
        (m_volume->get_width() + k_workgroup_x - 1) / k_workgroup_x,
        (m_volume->get_height() + k_workgroup_y - 1) / k_workgroup_y,
        (m_volume->get_depth() + k_workgroup_z - 1) / k_workgroup_z);
}

void VolumeFieldProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    m_volume = std::dynamic_pointer_cast<VolumeGridBuffer>(buffer);
    if (!m_volume) {
        return;
    }

    if (!validate_fields()) {
        m_volume.reset();
        return;
    }

    size_dispatch();
    reserve_param_size(sizeof(LatticeParams));
    write_lattice_params();

    on_volume_ready();
}

void VolumeFieldProcessor::write_lattice_params()
{
    if (!m_volume) {
        return;
    }

    auto& data = get_push_constant_data();
    if (data.size() < m_config.push_constant_size) {
        data.resize(m_config.push_constant_size);
    }

    const glm::vec3 cell = m_volume->get_cell_size();

    uint32_t preserved_3 = 0;
    float preserved_7 = 0.0F;
    std::memcpy(&preserved_3, data.data() + k_word3_offset, sizeof(uint32_t));
    std::memcpy(&preserved_7, data.data() + k_word7_offset, sizeof(float));

    const LatticeParams params {
        .width = m_volume->get_width(),
        .height = m_volume->get_height(),
        .depth = m_volume->get_depth(),
        .word3 = preserved_3,
        .cell_size_x = cell.x,
        .cell_size_y = cell.y,
        .cell_size_z = cell.z,
        .word7 = preserved_7,
    };

    std::memcpy(data.data(), &params, sizeof(LatticeParams));
}

void VolumeFieldProcessor::write_lattice_word3(uint32_t value)
{
    auto& data = get_push_constant_data();
    if (data.size() < sizeof(LatticeParams)) {
        return;
    }

    std::memcpy(data.data() + k_word3_offset, &value, sizeof(uint32_t));
}

void VolumeFieldProcessor::write_param_tail(size_t offset, const void* data_in, size_t size)
{
    if (offset < k_word7_offset) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "VolumeFieldProcessor: parameter offset {} overlaps the shared prefix",
            offset);
        return;
    }

    auto& data = get_push_constant_data();
    if (data.size() < offset + size) {
        data.resize(offset + size);
    }

    std::memcpy(data.data() + offset, data_in, size);
}

void VolumeFieldProcessor::write_field_descriptors()
{
    if (!m_volume || m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    for (const auto& entry : m_bindings) {
        const vk::Buffer handle = entry.access == FieldAccess::READ
            ? m_volume->read_handle(entry.field)
            : m_volume->write_handle(entry.field);

        foundry.update_descriptor_buffer(
            m_descriptor_set_ids[0], entry.binding, vk::DescriptorType::eStorageBuffer,
            handle, 0, m_volume->get_field_bytes(entry.field));
    }
}

void VolumeFieldProcessor::on_descriptors_created()
{
    write_field_descriptors();
}

void VolumeFieldProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_volume && are_descriptors_ready()) {
        write_field_descriptors();
    }

    ComputeProcessor::processing_function(buffer);

    if (!m_volume || !wants_swap()) {
        return;
    }

    for (const auto& field : m_swap_fields) {
        m_volume->swap_field(field);
    }
}

bool VolumeFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    return m_volume && std::dynamic_pointer_cast<VolumeGridBuffer>(buffer) != nullptr;
}

} // namespace MayaFlux::Buffers
