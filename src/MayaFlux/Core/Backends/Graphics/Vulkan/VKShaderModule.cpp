#include "VKShaderModule.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <fstream>

#ifdef MAYAFLUX_USE_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#ifdef MAYAFLUX_USE_SPIRV_REFLECT
#include <spirv_reflect.h>
#endif

namespace MayaFlux::Core {

namespace {
    std::vector<std::string> get_shader_search_paths()
    {
        return {
            SHADER_BUILD_OUTPUT_DIR, // 1. Build directory (development)
            SHADER_INSTALL_DIR, // 2. Install directory (production)
            SHADER_SOURCE_DIR, // 3. Source directory (fallback)
            "./shaders", // 4. Current working directory
            "../shaders" // 5. Parent directory
        };
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

    // if (spirv_code.size() % 4 != 0) {
    //     MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
    //         "Invalid SPIR-V code size: {} bytes (must be multiple of 4)",
    //         spirv_code.size());
    //     return false;
    // }

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
#ifdef MAYAFLUX_USE_SPIRV_REFLECT
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(
        spirv_code.size() * sizeof(uint32_t),
        spirv_code.data(),
        &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "SPIRV-Reflect failed: {}", static_cast<int>(result));
        return false;
    }

    uint32_t binding_count = 0;
    result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
    if (result == SPV_REFLECT_RESULT_SUCCESS && binding_count > 0) {
        std::vector<SpvReflectDescriptorBinding*> bindings(binding_count);
        spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());

        for (const auto* binding : bindings) {
            ShaderReflection::DescriptorBinding desc;
            desc.set = binding->set;
            desc.binding = binding->binding;
            desc.type = static_cast<vk::DescriptorType>(binding->descriptor_type);
            desc.stage = m_stage;
            desc.count = binding->count;
            desc.name = binding->name ? binding->name : "";

            m_reflection.bindings.push_back(desc);
        }

        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Reflected {} descriptor bindings", binding_count);
    }

    uint32_t push_constant_count = 0;
    result = spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, nullptr);
    if (result == SPV_REFLECT_RESULT_SUCCESS && push_constant_count > 0) {
        std::vector<SpvReflectBlockVariable*> push_constants(push_constant_count);
        spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, push_constants.data());

        for (const auto* block : push_constants) {
            ShaderReflection::PushConstantRange range;
            range.stage = m_stage;
            range.offset = block->offset;
            range.size = block->size;

            m_reflection.push_constants.push_back(range);
        }

        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Reflected {} push constant blocks", push_constant_count);
    }

    if (m_stage == vk::ShaderStageFlagBits::eCompute) {
        m_reflection.workgroup_size = std::array<uint32_t, 3> {
            module.entry_points[0].local_size.x,
            module.entry_points[0].local_size.y,
            module.entry_points[0].local_size.z
        };

        MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Compute shader workgroup size: [{}, {}, {}]",
            (*m_reflection.workgroup_size)[0],
            (*m_reflection.workgroup_size)[1],
            (*m_reflection.workgroup_size)[2]);
    }

    spvReflectDestroyShaderModule(&module);
    return true;

#else
    MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "SPIRV-Reflect not available - shader reflection disabled");

    if (m_stage == vk::ShaderStageFlagBits::eCompute) {
        // SPIR-V OpExecutionMode LocalSize pattern
        // This is a simplified parser - production code should use SPIRV-Reflect
        for (size_t i = 0; i < spirv_code.size() - 4; ++i) {
            if ((spirv_code[i] & 0xFFFF) == 17) {
                uint32_t mode = spirv_code[i + 2];
                if (mode == 17) {
                    m_reflection.workgroup_size = std::array<uint32_t, 3> {
                        spirv_code[i + 3],
                        spirv_code[i + 4],
                        spirv_code[i + 5]
                    };

                    MF_DEBUG(Journal::Component::Core, Journal::Context::GraphicsBackend,
                        "Extracted compute workgroup size: [{}, {}, {}]",
                        (*m_reflection.workgroup_size)[0],
                        (*m_reflection.workgroup_size)[1],
                        (*m_reflection.workgroup_size)[2]);
                    break;
                }
            }
        }
    }

    return false; // Reflection incomplete without SPIRV-Reflect
#endif
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
    MF_ERROR(Journal::Component::Core, Journal::Context::GraphicsBackend,
        "GLSL compilation requested but shaderc not available - use pre-compiled SPIR-V");
    return {};
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
    default:
        MF_WARN(Journal::Component::Core, Journal::Context::GraphicsBackend,
            "Unknown shader stage: {}", vk::to_string(m_stage));
        return Stage::COMPUTE;
    }
}

} // namespace MayaFlux::Core
