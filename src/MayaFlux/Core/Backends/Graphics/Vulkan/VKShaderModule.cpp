#include "VKShaderModule.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>

#ifdef MAYAFLUX_USE_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#include <spirv_cross/spirv_cross.hpp>

namespace MayaFlux::Core {

namespace {
    std::vector<std::string> get_shader_search_paths()
    {
        std::vector<std::string> paths {
            SHADER_BUILD_OUTPUT_DIR, // 1. Build directory (development)
            SHADER_INSTALL_DIR, // 2. Install directory (production)
            SHADER_SOURCE_DIR, // 3. Source directory (fallback)
            "./shaders", // 4. Current working directory
            "../shaders", // 5. Parent directory
            "data/shaders", // 6. Weave project root convention
            "./data/shaders", // 6. Weave project root convention
            "../data/shaders" // 7. if running from build/
        };

        if (std::string_view(SHADER_EXAMPLE_DIR).length() > 0) {
            paths.emplace_back(SHADER_EXAMPLE_DIR);
        }

#ifdef MAYAFLUX_PROJECT_SHADER_DIR
        paths.emplace_back(MAYAFLUX_PROJECT_SHADER_DIR);
#endif

        return paths;
    }

    std::string resolve_shader_path(const std::string& filename)
    {
        namespace fs = std::filesystem;

        if (fs::path(filename).is_absolute() || fs::exists(filename)) {
            return filename;
        }

        for (const auto& search_path : get_shader_search_paths()) {
            fs::path full_path = fs::path(search_path) / filename;
            if (fs::exists(full_path)) {
                MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                    "Resolved shader '{}' -> '{}'", filename, full_path.string());
                return full_path.string();
            }
        }

        return filename;
    }

    vk::DescriptorType spirv_to_vk_descriptor_type(spirv_cross::SPIRType::BaseType base_type,
        const spirv_cross::SPIRType& type,
        bool is_storage)
    {
        if (type.image.dim != spv::DimMax) {
            if (type.image.sampled == 2) {
                return vk::DescriptorType::eStorageImage;
            }

            if (type.image.sampled == 1) {
                return is_storage ? vk::DescriptorType::eSampledImage
                                  : vk::DescriptorType::eCombinedImageSampler;
            }
        }

        if (is_storage) {
            return vk::DescriptorType::eStorageBuffer;
        }

        return vk::DescriptorType::eUniformBuffer;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

VKShaderModule::~VKShaderModule()
{
    if (m_module) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "VKShaderModule destroyed without cleanup() - potential leak");
    }
}

VKShaderModule::VKShaderModule(VKShaderModule&& other) noexcept
    : m_module(other.m_module)
    , m_stage(other.m_stage)
    , m_entry_point(std::move(other.m_entry_point))
    , m_reflection(std::move(other.m_reflection))
    , m_spirv_code(std::move(other.m_spirv_code))
    , m_preserve_spirv(other.m_preserve_spirv)
    , m_specialization_map(std::move(other.m_specialization_map))
    , m_specialization_entries(std::move(other.m_specialization_entries))
    , m_specialization_data(std::move(other.m_specialization_data))
    , m_specialization_info(other.m_specialization_info)
{
    other.m_module = nullptr;
}

VKShaderModule& VKShaderModule::operator=(VKShaderModule&& other) noexcept
{
    if (this != &other) {
        if (m_module) {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "VKShaderModule move-assigned without cleanup() - potential leak");
        }

        m_module = other.m_module;
        m_stage = other.m_stage;
        m_entry_point = std::move(other.m_entry_point);
        m_reflection = std::move(other.m_reflection);
        m_spirv_code = std::move(other.m_spirv_code);
        m_preserve_spirv = other.m_preserve_spirv;
        m_specialization_map = std::move(other.m_specialization_map);
        m_specialization_entries = std::move(other.m_specialization_entries);
        m_specialization_data = std::move(other.m_specialization_data);
        m_specialization_info = other.m_specialization_info;

        other.m_module = nullptr;
    }
    return *this;
}

void VKShaderModule::cleanup(vk::Device device)
{
    if (m_module) {
        device.destroyShaderModule(m_module);
        m_module = nullptr;

        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Shader module cleaned up ({} stage)", vk::to_string(m_stage));
    }

    m_spirv_code.clear();
    m_reflection = ShaderReflection {};
    m_specialization_map.clear();
    m_specialization_entries.clear();
    m_specialization_data.clear();
}

// ============================================================================
// Creation from SPIR-V
// ============================================================================

bool VKShaderModule::create_from_spirv(
    vk::Device device,
    const std::vector<uint32_t>& spirv_code,
    vk::ShaderStageFlagBits stage,
    const std::string& entry_point,
    bool enable_reflection)
{
    if (spirv_code.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot create shader module from empty SPIR-V code");
        return false;
    }

    if (spirv_code[0] != 0x07230203) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Invalid SPIR-V magic number: 0x{:08X} (expected 0x07230203)",
            spirv_code[0]);
        return false;
    }

    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
    create_info.pCode = spirv_code.data();

    try {
        m_module = device.createShaderModule(create_info);
    } catch (const vk::SystemError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to create shader module: {}", e.what());
        return false;
    }

    m_stage = stage;
    m_entry_point = entry_point;

    if (m_preserve_spirv) {
        m_spirv_code = spirv_code;
    }

    if (enable_reflection) {
        if (!reflect_spirv(spirv_code)) {
            MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Shader reflection failed - descriptor layouts must be manually specified");
        }
    }

    MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Shader module created ({} stage, {} bytes SPIR-V, entry='{}')",
        vk::to_string(stage), spirv_code.size() * 4, entry_point);

    return true;
}

bool VKShaderModule::create_from_spirv_file(
    vk::Device device,
    const std::string& spirv_path,
    vk::ShaderStageFlagBits stage,
    const std::string& entry_point,
    bool enable_reflection)
{
    std::string resolved_path = resolve_shader_path(spirv_path);

    auto spirv_code = read_spirv_file(resolved_path);
    if (spirv_code.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to read SPIR-V file: '{}'", spirv_path);
        return false;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Loaded SPIR-V from file: '{}'", spirv_path);

    return create_from_spirv(device, spirv_code, stage, entry_point, enable_reflection);
}

// ============================================================================
// Creation from GLSL
// ============================================================================

bool VKShaderModule::create_from_glsl(
    vk::Device device,
    const std::string& glsl_source,
    vk::ShaderStageFlagBits stage,
    const std::string& entry_point,
    bool enable_reflection,
    const std::vector<std::string>& include_directories,
    const std::unordered_map<std::string, std::string>& defines)
{
    auto spirv_code = compile_glsl_to_spirv(glsl_source, stage, include_directories, defines);
    if (spirv_code.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to compile GLSL to SPIR-V ({} stage)", vk::to_string(stage));
        return false;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Compiled GLSL to SPIR-V ({} stage, {} bytes)",
        vk::to_string(stage), spirv_code.size() * 4);

    return create_from_spirv(device, spirv_code, stage, entry_point, enable_reflection);
}

bool VKShaderModule::create_from_glsl_file(
    vk::Device device,
    const std::string& glsl_path,
    std::optional<vk::ShaderStageFlagBits> stage,
    const std::string& entry_point,
    bool enable_reflection,
    const std::vector<std::string>& include_directories,
    const std::unordered_map<std::string, std::string>& defines)
{
    std::string resolved_path = resolve_shader_path(glsl_path);

    if (!stage.has_value()) {
        stage = detect_stage_from_extension(glsl_path);
        if (!stage.has_value()) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Cannot auto-detect shader stage from file extension: '{}'", glsl_path);
            return false;
        }
        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Auto-detected {} stage from file extension", vk::to_string(*stage));
    }

    auto glsl_source = read_text_file(resolved_path);
    if (glsl_source.empty()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to read GLSL file: '{}'", glsl_path);
        return false;
    }

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Loaded GLSL from file: '{}' ({} bytes)", glsl_path, glsl_source.size());

    return create_from_glsl(device, glsl_source, *stage, entry_point,
        enable_reflection, include_directories, defines);
}

// ============================================================================
// Pipeline Integration
// ============================================================================

vk::PipelineShaderStageCreateInfo VKShaderModule::get_stage_create_info() const
{
    if (!m_module) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Cannot get stage create info from invalid shader module");
        return {};
    }

    vk::PipelineShaderStageCreateInfo stage_info;
    stage_info.stage = m_stage;
    stage_info.module = m_module;
    stage_info.pName = m_entry_point.c_str();

    if (!m_specialization_entries.empty()) {
        const_cast<VKShaderModule*>(this)->update_specialization_info();
        stage_info.pSpecializationInfo = &m_specialization_info;
    }

    return stage_info;
}

// ============================================================================
// Specialization Constants
// ============================================================================

void VKShaderModule::set_specialization_constants(
    const std::unordered_map<uint32_t, uint32_t>& constants)
{
    m_specialization_map = constants;

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Set {} specialization constants for {} stage",
        constants.size(), vk::to_string(m_stage));
}

void VKShaderModule::update_specialization_info()
{
    if (m_specialization_map.empty()) {
        m_specialization_entries.clear();
        m_specialization_data.clear();
        m_specialization_info = vk::SpecializationInfo {};
        return;
    }

    m_specialization_entries.clear();
    m_specialization_data.clear();

    m_specialization_entries.reserve(m_specialization_map.size());
    m_specialization_data.reserve(m_specialization_map.size());

    uint32_t offset = 0;
    for (const auto& [constant_id, value] : m_specialization_map) {
        vk::SpecializationMapEntry entry;
        entry.constantID = constant_id;
        entry.offset = offset;
        entry.size = sizeof(uint32_t);
        m_specialization_entries.push_back(entry);

        m_specialization_data.push_back(value);
        offset += sizeof(uint32_t);
    }

    m_specialization_info.mapEntryCount = static_cast<uint32_t>(m_specialization_entries.size());
    m_specialization_info.pMapEntries = m_specialization_entries.data();
    m_specialization_info.dataSize = m_specialization_data.size() * sizeof(uint32_t);
    m_specialization_info.pData = m_specialization_data.data();
}

// ============================================================================
// Reflection
// ============================================================================

bool VKShaderModule::reflect_spirv(const std::vector<uint32_t>& spirv_code)
{
    try {
        spirv_cross::Compiler compiler(spirv_code);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        auto reflect_resources = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& res_vec,
                                     bool is_storage = false) {
            for (const auto& resource : res_vec) {
                ShaderReflection::DescriptorBinding desc;
                desc.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                desc.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                desc.stage = m_stage;
                desc.name = resource.name;

                const auto& type = compiler.get_type(resource.type_id);
                desc.count = type.array.empty() ? 1 : type.array[0];
                desc.type = spirv_to_vk_descriptor_type(type.basetype, type, is_storage);

                m_reflection.bindings.push_back(desc);
            }
        };

        reflect_resources(resources.uniform_buffers, false);

        reflect_resources(resources.storage_buffers, true);

        reflect_resources(resources.sampled_images, false);

        reflect_resources(resources.storage_images, true);

        reflect_resources(resources.separate_images, false);
        reflect_resources(resources.separate_samplers, false);

        if (!m_reflection.bindings.empty()) {
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Reflected {} descriptor bindings", m_reflection.bindings.size());
        }

        for (const auto& pc_buffer : resources.push_constant_buffers) {
            const auto& type = compiler.get_type(pc_buffer.type_id);

            ShaderReflection::PushConstantRange range;
            range.stage = m_stage;
            range.offset = 0;
            range.size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            m_reflection.push_constants.push_back(range);
        }

        if (!m_reflection.push_constants.empty()) {
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Reflected {} push constant blocks", m_reflection.push_constants.size());
        }

        auto spec_constants = compiler.get_specialization_constants();
        for (const auto& spec : spec_constants) {
            ShaderReflection::SpecializationConstant sc;
            sc.constant_id = spec.constant_id;
            sc.name = compiler.get_name(spec.id);

            const auto& type = compiler.get_type(compiler.get_constant(spec.id).constant_type);
            sc.size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            m_reflection.specialization_constants.push_back(sc);
        }

        if (!m_reflection.specialization_constants.empty()) {
            MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Reflected {} specialization constants",
                m_reflection.specialization_constants.size());
        }

        if (m_stage == vk::ShaderStageFlagBits::eCompute
            || m_stage == vk::ShaderStageFlagBits::eMeshEXT
            || m_stage == vk::ShaderStageFlagBits::eTaskEXT) {
            auto entry_points = compiler.get_entry_points_and_stages();

            for (const auto& ep : entry_points) {
                if (ep.name == m_entry_point && ep.execution_model == spv::ExecutionModelGLCompute) {

                    std::array<uint32_t, 3> workgroup_size {
                        compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0),
                        compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1),
                        compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2)
                    };

                    if (!workgroup_size.empty() && workgroup_size.size() >= 3) {
                        m_reflection.workgroup_size = std::array<uint32_t, 3> {
                            workgroup_size[0],
                            workgroup_size[1],
                            workgroup_size[2]
                        };

                        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                            "Compute shader workgroup size: [{}, {}, {}]",
                            workgroup_size[0], workgroup_size[1], workgroup_size[2]);
                    }
                    break;
                }
            }
        }

        if (m_stage == vk::ShaderStageFlagBits::eVertex) {
            for (const auto& input : resources.stage_inputs) {
                uint32_t location = compiler.get_decoration(input.id, spv::DecorationLocation);
                const auto& type = compiler.get_type(input.type_id);

                vk::VertexInputAttributeDescription attr;
                attr.location = location;
                attr.binding = 0;
                attr.format = spirv_type_to_vk_format(type);
                attr.offset = 0;

                m_reflection.vertex_attributes.push_back(attr);
            }

            if (!m_reflection.vertex_attributes.empty()) {
                MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                    "Reflected {} vertex input attributes",
                    m_reflection.vertex_attributes.size());
            }
        }

        return true;

    } catch (const spirv_cross::CompilerError& e) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "SPIRV-Cross reflection failed: {}", e.what());
        return false;
    }
}

vk::Format VKShaderModule::spirv_type_to_vk_format(const spirv_cross::SPIRType& type)
{
    using BaseType = spirv_cross::SPIRType::BaseType;

    if (type.columns > 1) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Matrix types are not valid vertex attributes (columns={})",
            type.columns);
        return vk::Format::eUndefined;
    }

    if (type.width != 32) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Unsupported SPIR-V vertex attribute width {} (only 32-bit supported)",
            type.width);
        return vk::Format::eUndefined;
    }

    const uint32_t vec_size = type.vecsize;
    if (type.basetype == BaseType::Float) {
        switch (vec_size) {
        case 1:
            return vk::Format::eR32Sfloat;
        case 2:
            return vk::Format::eR32G32Sfloat;
        case 3:
            return vk::Format::eR32G32B32Sfloat;
        case 4:
            return vk::Format::eR32G32B32A32Sfloat;
        }
    } else if (type.basetype == BaseType::Int) {
        switch (vec_size) {
        case 1:
            return vk::Format::eR32Sint;
        case 2:
            return vk::Format::eR32G32Sint;
        case 3:
            return vk::Format::eR32G32B32Sint;
        case 4:
            return vk::Format::eR32G32B32A32Sint;
        }
    } else if (type.basetype == BaseType::UInt) {
        switch (vec_size) {
        case 1:
            return vk::Format::eR32Uint;
        case 2:
            return vk::Format::eR32G32Uint;
        case 3:
            return vk::Format::eR32G32B32Uint;
        case 4:
            return vk::Format::eR32G32B32A32Uint;
        }
    }

    MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Unsupported SPIR-V vertex attribute type (basetype={}, vecsize={})",
        static_cast<int>(type.basetype), vec_size);

    return vk::Format::eUndefined;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::optional<vk::ShaderStageFlagBits> VKShaderModule::detect_stage_from_extension(
    const std::string& filepath)
{
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::unordered_map<std::string, vk::ShaderStageFlagBits> extension_map = {
        { ".comp", vk::ShaderStageFlagBits::eCompute },
        { ".vert", vk::ShaderStageFlagBits::eVertex },
        { ".frag", vk::ShaderStageFlagBits::eFragment },
        { ".geom", vk::ShaderStageFlagBits::eGeometry },
        { ".tesc", vk::ShaderStageFlagBits::eTessellationControl },
        { ".tese", vk::ShaderStageFlagBits::eTessellationEvaluation },
        { ".rgen", vk::ShaderStageFlagBits::eRaygenKHR },
        { ".rint", vk::ShaderStageFlagBits::eIntersectionKHR },
        { ".rahit", vk::ShaderStageFlagBits::eAnyHitKHR },
        { ".rchit", vk::ShaderStageFlagBits::eClosestHitKHR },
        { ".rmiss", vk::ShaderStageFlagBits::eMissKHR },
        { ".rcall", vk::ShaderStageFlagBits::eCallableKHR },
        { ".mesh", vk::ShaderStageFlagBits::eMeshEXT },
        { ".task", vk::ShaderStageFlagBits::eTaskEXT }
    };

    auto it = extension_map.find(ext);
    if (it != extension_map.end()) {
        return it->second;
    }

    return std::nullopt;
}

std::vector<uint32_t> VKShaderModule::compile_glsl_to_spirv(
    const std::string& glsl_source,
    vk::ShaderStageFlagBits stage,
    const std::vector<std::string>& include_directories,
    const std::unordered_map<std::string, std::string>& defines)
{
#ifdef MAYAFLUX_USE_SHADERC
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    for (const auto& dir : include_directories) {
        // options.AddIncludeDirectory(dir);
    }

    for (const auto& [name, value] : defines) {
        options.AddMacroDefinition(name, value);
    }

    shaderc_shader_kind shader_kind;
    switch (stage) {
    case vk::ShaderStageFlagBits::eVertex:
        shader_kind = shaderc_glsl_vertex_shader;
        break;
    case vk::ShaderStageFlagBits::eFragment:
        shader_kind = shaderc_glsl_fragment_shader;
        break;
    case vk::ShaderStageFlagBits::eCompute:
        shader_kind = shaderc_glsl_compute_shader;
        break;
    case vk::ShaderStageFlagBits::eGeometry:
        shader_kind = shaderc_glsl_geometry_shader;
        break;
    case vk::ShaderStageFlagBits::eTessellationControl:
        shader_kind = shaderc_glsl_tess_control_shader;
        break;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        shader_kind = shaderc_glsl_tess_evaluation_shader;
        break;
    case vk::ShaderStageFlagBits::eMeshEXT:
        shader_kind = shaderc_glsl_mesh_shader;
        break;
    case vk::ShaderStageFlagBits::eTaskEXT:
        shader_kind = shaderc_glsl_task_shader;
        break;
    default:
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Unsupported shader stage for GLSL compilation: {}", vk::to_string(stage));
        return {};
    }

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        glsl_source,
        shader_kind,
        "shader.glsl",
        options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "GLSL compilation failed:\n{}", result.GetErrorMessage());
        return {};
    }

    std::vector<uint32_t> spirv(result.cbegin(), result.cend());

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Compiled GLSL ({} stage) -> {} bytes SPIR-V",
        vk::to_string(stage), spirv.size() * 4);

    return spirv;

#else
    return compile_glsl_to_spirv_external(glsl_source, stage, include_directories, defines);
#endif
}

std::vector<uint32_t> VKShaderModule::read_spirv_file(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to open SPIR-V file: '{}'", filepath);
        return {};
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    if (file_size == 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "SPIR-V file is empty: '{}'", filepath);
        return {};
    }

    if (file_size % sizeof(uint32_t) != 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "SPIR-V file size ({} bytes) is not multiple of 4: '{}'",
            file_size, filepath);
        return {};
    }

    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    if (!file) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to read SPIR-V file: '{}'", filepath);
        return {};
    }

    return buffer;
}

std::string VKShaderModule::read_text_file(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Failed to open file: '{}'", filepath);
        return {};
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    if (content.empty()) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "File is empty: '{}'", filepath);
    }

    return content;
}

#ifndef MAYAFLUX_USE_SHADERC
namespace {
    bool is_command_available(const std::string& command)
    {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        std::string check_cmd = "where " + command + " >nul 2>&1";
#else
        std::string check_cmd = "command -v " + command + " >/dev/null 2>&1";
#endif
        return std::system(check_cmd.c_str()) == 0;
    }

    std::string get_null_device()
    {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        return "nul";
#else
        return "/dev/null";
#endif
    }
}

std::vector<uint32_t> VKShaderModule::compile_glsl_to_spirv_external(
    const std::string& glsl_source,
    vk::ShaderStageFlagBits stage,
    const std::vector<std::string>& include_directories,
    const std::unordered_map<std::string, std::string>& defines)
{
    namespace fs = std::filesystem;

    if (!is_command_available("glslc")) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "glslc compiler not found in PATH. Install Vulkan SDK or enable MAYAFLUX_USE_SHADERC");
        return {};
    }

    fs::path temp_dir = fs::temp_directory_path();
    fs::path glsl_temp = temp_dir / "mayaflux_shader_temp.glsl";
    fs::path spirv_temp = temp_dir / "mayaflux_shader_temp.spv";

    {
        std::ofstream out(glsl_temp);
        if (!out.is_open()) {
            MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
                "Failed to create temporary GLSL file: '{}'", glsl_temp.string());
            return {};
        }
        out << glsl_source;
    }

    std::string stage_flag;
    switch (stage) {
    case vk::ShaderStageFlagBits::eVertex:
        stage_flag = "-fshader-stage=vertex";
        break;
    case vk::ShaderStageFlagBits::eFragment:
        stage_flag = "-fshader-stage=fragment";
        break;
    case vk::ShaderStageFlagBits::eCompute:
        stage_flag = "-fshader-stage=compute";
        break;
    case vk::ShaderStageFlagBits::eGeometry:
        stage_flag = "-fshader-stage=geometry";
        break;
    case vk::ShaderStageFlagBits::eTessellationControl:
        stage_flag = "-fshader-stage=tesscontrol";
        break;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        stage_flag = "-fshader-stage=tesseval";
        break;
    case vk::ShaderStageFlagBits::eMeshEXT:
        stage_flag = "-fshader-stage=mesh";
        break;
    case vk::ShaderStageFlagBits::eTaskEXT:
        stage_flag = "-fshader-stage=task";
        break;
    default:
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Unsupported shader stage for external compilation: {}", vk::to_string(stage));
        fs::remove(glsl_temp);
        return {};
    }

#if defined(MAYAFLUX_PLATFORM_WINDOWS)
    std::string cmd = "glslc " + stage_flag + " \"" + glsl_temp.string() + "\" -o \"" + spirv_temp.string() + "\"";
#else
    std::string cmd = "glslc " + stage_flag + " '" + glsl_temp.string() + "' -o '" + spirv_temp.string() + "'";
#endif

    for (const auto& dir : include_directories) {
#if defined(MAYAFLUX_PLATFORM_WINDOWS)
        cmd += " -I\"" + dir + "\"";
#else
        cmd += " -I'" + dir + "'";
#endif
    }

    for (const auto& [name, value] : defines) {
        cmd += " -D" + name;
        if (!value.empty()) {
            cmd += "=" + value;
        }
    }

    std::string null_device = get_null_device();

    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "Invoking external glslc : {}", cmd);

    int result = std::system(cmd.c_str());

    fs::remove(glsl_temp);

    if (result != 0) {
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "External glslc compilation failed (exit code: {})", result);
        MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Make sure Vulkan SDK is installed or enable MAYAFLUX_USE_SHADERC for runtime compilation");
        fs::remove(spirv_temp);
        return {};
    }

    auto spirv_code = read_spirv_file(spirv_temp.string());

    fs::remove(spirv_temp);

    if (!spirv_code.empty()) {
        MF_INFO(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Compiled GLSL ({} stage) via external glslc -> {} bytes SPIR-V",
            vk::to_string(stage), spirv_code.size() * 4);
    }

    return spirv_code;
}
#endif

Stage VKShaderModule::get_stage_type() const
{
    switch (m_stage) {
    case vk::ShaderStageFlagBits::eCompute:
        return Stage::COMPUTE;
    case vk::ShaderStageFlagBits::eVertex:
        return Stage::VERTEX;
    case vk::ShaderStageFlagBits::eFragment:
        return Stage::FRAGMENT;
    case vk::ShaderStageFlagBits::eGeometry:
        return Stage::GEOMETRY;
    case vk::ShaderStageFlagBits::eTessellationControl:
        return Stage::TESS_CONTROL;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return Stage::TESS_EVALUATION;
    case vk::ShaderStageFlagBits::eMeshEXT:
        return Stage::MESH;
    case vk::ShaderStageFlagBits::eTaskEXT:
        return Stage::TASK;
    default:
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Unknown shader stage: {}", vk::to_string(m_stage));
        return Stage::COMPUTE;
    }
}

} // namespace MayaFlux::Core
