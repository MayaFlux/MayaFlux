#pragma once

#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {

/**
 * @enum ShaderType
 * @brief High-level shader type enumeration
 *
 * Used for specifying shader types in a generic way.
 */
enum class Stage : uint8_t {
    COMPUTE,
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    TESS_CONTROL,
    TESS_EVALUATION
};

struct FragmentOutputState {
    std::vector<vk::Format> color_formats;
    vk::Format depth_format;
    vk::Format stencil_format;
};

struct VertexInputState {
    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attributes;
};

struct VertexInputInfo {
    struct Attribute {
        uint32_t location; // layout(location = N)
        vk::Format format; // vec3 -> eR32G32B32Sfloat
        uint32_t offset; // byte offset in vertex
        std::string name; // variable name (from reflection)
    };

    struct Binding {
        uint32_t binding; // vertex buffer binding point
        uint32_t stride; // bytes per vertex
        vk::VertexInputRate rate; // per-vertex or per-instance
    };

    std::vector<Attribute> attributes;
    std::vector<Binding> bindings;
};

struct FragmentOutputInfo {
    struct Attachment {
        uint32_t location; // layout(location = N)
        vk::Format format; // vec4 -> eR32G32B32A32Sfloat
        std::string name; // output variable name
    };

    std::vector<Attachment> color_attachments;
    bool has_depth_output = false;
    bool has_stencil_output = false;
};

struct PushConstantInfo {
    uint32_t offset;
    uint32_t size;
    std::string name; // struct name (if any)
    vk::ShaderStageFlags stages; // which stages use it
};

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

    /**
     * @brief Get shader stage type
     * @return Stage enum (easier than vk::ShaderStageFlagBits for logic)
     */
    [[nodiscard]] Stage get_stage_type() const;

    /**
     * @brief Get vertex input state (vertex shaders only)
     * @return Vertex input metadata, empty if not a vertex shader
     */
    [[nodiscard]] const VertexInputInfo& get_vertex_input() const
    {
        return m_vertex_input;
    }

    /**
     * @brief Check if vertex input is available
     */
    [[nodiscard]] bool has_vertex_input() const
    {
        return !m_vertex_input.attributes.empty();
    }

    /**
     * @brief Get fragment output state (fragment shaders only)
     * @return Fragment output metadata, empty if not a fragment shader
     */
    [[nodiscard]] const FragmentOutputInfo& get_fragment_output() const
    {
        return m_fragment_output;
    }

    /**
     * @brief Get detailed push constant info
     * @return Push constant metadata (replaces simple PushConstantRange)
     */
    [[nodiscard]] const std::vector<PushConstantInfo>& get_push_constants() const
    {
        return m_push_constants;
    }

    // NEW: Workgroup size for compute shaders
    /**
     * @brief Get compute workgroup size (compute shaders only)
     * @return {local_size_x, local_size_y, local_size_z} or nullopt
     */
    [[nodiscard]] std::optional<std::array<uint32_t, 3>> get_workgroup_size() const
    {
        return m_reflection.workgroup_size;
    }

private:
    vk::ShaderModule m_module = nullptr;
    vk::ShaderStageFlagBits m_stage = vk::ShaderStageFlagBits::eCompute;
    std::string m_entry_point = "main";

    ShaderReflection m_reflection;
    std::vector<uint32_t> m_spirv_code; ///< Preserved SPIR-V (if enabled)

    bool m_preserve_spirv {};

    std::unordered_map<uint32_t, uint32_t> m_specialization_map;
    std::vector<vk::SpecializationMapEntry> m_specialization_entries;
    std::vector<uint32_t> m_specialization_data;
    vk::SpecializationInfo m_specialization_info;

    VertexInputInfo m_vertex_input;
    FragmentOutputInfo m_fragment_output;
    std::vector<PushConstantInfo> m_push_constants;

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
