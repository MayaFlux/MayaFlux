#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @struct ShaderReflection
 * @brief Metadata extracted from shader module
 *
 * Contains information about shader resources for descriptor set layout creation
 * and pipeline configuration. Extracted via SPIRV-Reflect or manual parsing.
 */
struct ShaderReflection {
    struct DescriptorBinding {
        uint32_t set; ///< Descriptor set index
        uint32_t binding; ///< Binding point within set
        vk::DescriptorType type; ///< Type (uniform buffer, storage buffer, etc.)
        vk::ShaderStageFlags stage; ///< Stage visibility
        uint32_t count; ///< Array size (1 for non-arrays)
        std::string name; ///< Variable name in shader
    };

    struct PushConstantRange {
        vk::ShaderStageFlags stage; ///< Stage visibility
        uint32_t offset; ///< Offset in push constant block
        uint32_t size; ///< Size in bytes
    };

    struct SpecializationConstant {
        uint32_t constant_id; ///< Specialization constant ID
        uint32_t size; ///< Size in bytes
        std::string name; ///< Variable name in shader
    };

    std::vector<DescriptorBinding> bindings;
    std::vector<PushConstantRange> push_constants;
    std::vector<SpecializationConstant> specialization_constants;

    std::optional<std::array<uint32_t, 3>> workgroup_size; ///< local_size_x/y/z

    std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    std::vector<vk::VertexInputBindingDescription> vertex_bindings;
};

/**
 * @class VKShaderModule
 * @brief Wrapper for Vulkan shader module with lifecycle and reflection
 *
 * Responsibilities:
 * - Create vk::ShaderModule from SPIR-V binary or GLSL source
 * - Load shaders from disk or memory
 * - Extract shader metadata via reflection
 * - Provide pipeline stage info for pipeline creation
 * - Enable hot-reload support (recreation)
 *
 * Does NOT handle:
 * - Pipeline creation (that's VKComputePipeline/VKGraphicsPipeline)
 * - Descriptor set allocation (that's VKDescriptorManager)
 * - Shader compilation (delegates to external compiler)
 *
 * Integration points:
 * - VKComputePipeline/VKGraphicsPipeline: uses get_stage_create_info()
 * - VKDescriptorManager: uses get_reflection() for layout creation
 * - VKBufferProcessor: subclasses use this to load compute shaders
 */
class MAYAFLUX_API VKShaderModule {
public:
    VKShaderModule() = default;
    ~VKShaderModule();

    VKShaderModule(const VKShaderModule&) = delete;
    VKShaderModule& operator=(const VKShaderModule&) = delete;
    VKShaderModule(VKShaderModule&&) noexcept;
    VKShaderModule& operator=(VKShaderModule&&) noexcept;

    /**
     * @brief Create shader module from SPIR-V binary
     * @param device Logical device
     * @param spirv_code SPIR-V bytecode (must be aligned to uint32_t)
     * @param stage Shader stage (compute, vertex, fragment, etc.)
     * @param entry_point Entry point function name (default: "main")
     * @param enable_reflection Extract descriptor bindings and resources
     * @return true if creation succeeded
     *
     * This is the lowest-level creation method. All other create methods
     * eventually funnel through this one.
     */
    bool create_from_spirv(
        vk::Device device,
        const std::vector<uint32_t>& spirv_code,
        vk::ShaderStageFlagBits stage,
        const std::string& entry_point = "main",
        bool enable_reflection = true);

    /**
     * @brief Create shader module from SPIR-V file
     * @param device Logical device
     * @param spirv_path Path to .spv file
     * @param stage Shader stage
     * @param entry_point Entry point function name
     * @param enable_reflection Extract metadata
     * @return true if creation succeeded
     *
     * Reads binary file and calls create_from_spirv().
     */
    bool create_from_spirv_file(
        vk::Device device,
        const std::string& spirv_path,
        vk::ShaderStageFlagBits stage,
        const std::string& entry_point = "main",
        bool enable_reflection = true);

    /**
     * @brief Create shader module from GLSL source string
     * @param device Logical device
     * @param glsl_source GLSL source code
     * @param stage Shader stage (determines compiler mode)
     * @param entry_point Entry point function name
     * @param enable_reflection Extract metadata
     * @param include_directories Paths for #include resolution
     * @param defines Preprocessor definitions (e.g., {"DEBUG", "MAX_LIGHTS=4"})
     * @return true if creation succeeded
     *
     * Compiles GLSL → SPIR-V using shaderc, then calls create_from_spirv().
     * Requires shaderc library to be available.
     */
    bool create_from_glsl(
        vk::Device device,
        const std::string& glsl_source,
        vk::ShaderStageFlagBits stage,
        const std::string& entry_point = "main",
        bool enable_reflection = true,
        const std::vector<std::string>& include_directories = {},
        const std::unordered_map<std::string, std::string>& defines = {});

    /**
     * @brief Create shader module from GLSL file
     * @param device Logical device
     * @param glsl_path Path to .comp/.vert/.frag/.geom file
     * @param stage Shader stage (auto-detected from extension if not specified)
     * @param entry_point Entry point function name
     * @param enable_reflection Extract metadata
     * @param include_directories Paths for #include resolution
     * @param defines Preprocessor definitions
     * @return true if creation succeeded
     *
     * Reads file, compiles GLSL → SPIR-V, calls create_from_spirv().
     * Stage auto-detection:
     *   .comp → Compute
     *   .vert → Vertex
     *   .frag → Fragment
     *   .geom → Geometry
     *   .tesc → Tessellation Control
     *   .tese → Tessellation Evaluation
     */
    bool create_from_glsl_file(
        vk::Device device,
        const std::string& glsl_path,
        std::optional<vk::ShaderStageFlagBits> stage = std::nullopt,
        const std::string& entry_point = "main",
        bool enable_reflection = true,
        const std::vector<std::string>& include_directories = {},
        const std::unordered_map<std::string, std::string>& defines = {});

    /**
     * @brief Cleanup shader module
     * @param device Logical device (must match creation device)
     *
     * Destroys vk::ShaderModule and clears metadata.
     * Safe to call multiple times or on uninitialized modules.
     */
    void cleanup(vk::Device device);

    /**
     * @brief Check if module is valid
     * @return true if shader module was successfully created
     */
    [[nodiscard]] bool is_valid() const { return m_module != nullptr; }

    /**
     * @brief Get raw Vulkan shader module handle
     * @return vk::ShaderModule handle
     */
    [[nodiscard]] vk::ShaderModule get() const { return m_module; }

    /**
     * @brief Get shader stage
     * @return Stage flags (compute, vertex, fragment, etc.)
     */
    [[nodiscard]] vk::ShaderStageFlagBits get_stage() const { return m_stage; }

    /**
     * @brief Get entry point function name
     * @return Entry point string (typically "main")
     */
    [[nodiscard]] const std::string& get_entry_point() const { return m_entry_point; }

    /**
     * @brief Get pipeline shader stage create info
     * @return vk::PipelineShaderStageCreateInfo for pipeline creation
     *
     * This is the primary integration point with pipeline builders.
     * Usage:
     *   auto stage_info = shader_module.get_stage_create_info();
     *   pipeline_builder.add_shader_stage(stage_info);
     */
    [[nodiscard]] vk::PipelineShaderStageCreateInfo get_stage_create_info() const;

    /**
     * @brief Get shader reflection metadata
     * @return Const reference to extracted metadata
     *
     * Used by descriptor managers and pipeline builders to automatically
     * configure layouts and bindings without manual specification.
     */
    [[nodiscard]] const ShaderReflection& get_reflection() const { return m_reflection; }

    /**
     * @brief Get SPIR-V bytecode
     * @return Vector of SPIR-V words (empty if not preserved)
     *
     * Useful for caching, serialization, or re-creation.
     * Only available if preserve_spirv was enabled during creation.
     */
    [[nodiscard]] const std::vector<uint32_t>& get_spirv() const { return m_spirv_code; }

    /**
     * @brief Set specialization constants
     * @param constants Map of constant_id → value
     *
     * Updates the specialization info used in get_stage_create_info().
     * Must be called before using the shader in pipeline creation.
     *
     * Example:
     *   shader.set_specialization_constants({
     *       {0, 256},  // WORKGROUP_SIZE = 256
     *       {1, 1}     // ENABLE_OPTIMIZATION = true
     *   });
     */
    void set_specialization_constants(const std::unordered_map<uint32_t, uint32_t>& constants);

    /**
     * @brief Enable SPIR-V preservation for hot-reload
     * @param preserve If true, stores SPIR-V bytecode internally
     *
     * Increases memory usage but enables recreation without recompilation.
     */
    void set_preserve_spirv(bool preserve) { m_preserve_spirv = preserve; }

private:
    vk::ShaderModule m_module = nullptr;
    vk::ShaderStageFlagBits m_stage = vk::ShaderStageFlagBits::eCompute;
    std::string m_entry_point = "main";

    ShaderReflection m_reflection;
    std::vector<uint32_t> m_spirv_code; ///< Preserved SPIR-V (if enabled)

    bool m_preserve_spirv = false;

    std::unordered_map<uint32_t, uint32_t> m_specialization_map;
    std::vector<vk::SpecializationMapEntry> m_specialization_entries;
    std::vector<uint32_t> m_specialization_data;
    vk::SpecializationInfo m_specialization_info;

    /**
     * @brief Perform reflection on SPIR-V bytecode
     * @param spirv_code SPIR-V bytecode
     * @return true if reflection succeeded
     *
     * Uses SPIRV-Reflect library to extract bindings, push constants,
     * workgroup sizes, etc. Falls back to basic parsing if library unavailable.
     */
    bool reflect_spirv(const std::vector<uint32_t>& spirv_code);

    /**
     * @brief Auto-detect shader stage from file extension
     * @param filepath Path to shader file
     * @return Detected stage, or nullopt if unknown extension
     */
    static std::optional<vk::ShaderStageFlagBits> detect_stage_from_extension(const std::string& filepath);

    /**
     * @brief Compile GLSL to SPIR-V using shaderc
     * @param glsl_source GLSL source code
     * @param stage Shader stage (affects compiler settings)
     * @param include_directories Include paths
     * @param defines Preprocessor macros
     * @return SPIR-V bytecode, or empty vector on failure
     */
    std::vector<uint32_t> compile_glsl_to_spirv(
        const std::string& glsl_source,
        vk::ShaderStageFlagBits stage,
        const std::vector<std::string>& include_directories,
        const std::unordered_map<std::string, std::string>& defines);

    /**
     * @brief Read binary file into vector
     * @param filepath Path to file
     * @return File contents, or empty vector on failure
     */
    static std::vector<uint32_t> read_spirv_file(const std::string& filepath);

    /**
     * @brief Read text file into string
     * @param filepath Path to file
     * @return File contents, or empty string on failure
     */
    static std::string read_text_file(const std::string& filepath);

    /**
     * @brief Update specialization info from current map
     * Called before get_stage_create_info() to ensure fresh data
     */
    void update_specialization_info();
};

} // namespace MayaFlux::Core
