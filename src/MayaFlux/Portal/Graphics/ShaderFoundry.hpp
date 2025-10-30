#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace MayaFlux::Core {
class VulkanBackend;
class VKShaderModule;
}

namespace MayaFlux::Portal::Graphics {

/**
 * @enum ShaderStage
 * @brief User-friendly shader stage enum
 *
 * Abstracts vk::ShaderStageFlagBits for Portal API convenience.
 * Maps directly to Vulkan stages internally.
 */
enum class ShaderStage : uint8_t {
    COMPUTE,
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    TESS_CONTROL,
    TESS_EVALUATION
};

/**
 * @struct ShaderCompilerConfig
 * @brief Configuration for shader compilation
 */
struct ShaderCompilerConfig {
    bool enable_optimization = true;
    bool enable_debug_info = false; ///< Include debug symbols (line numbers, variable names)
    bool enable_reflection = true; ///< Extract descriptor bindings and metadata
    bool enable_validation = true; ///< Validate SPIR-V after compilation
    std::vector<std::string> include_directories; ///< Paths for #include resolution
    std::unordered_map<std::string, std::string> defines; ///< Preprocessor macros

    ShaderCompilerConfig() = default;
};

/**
 * @struct ShaderSource
 * @brief Shader source descriptor for compilation
 */
struct ShaderSource {
    std::string content; ///< Shader source code or SPIR-V path
    ShaderStage stage;
    std::string entry_point = "main";

    enum class SourceType : uint8_t {
        GLSL_STRING, ///< In-memory GLSL source
        GLSL_FILE, ///< Path to .comp/.vert/.frag/etc
        SPIRV_FILE ///< Path to .spv file
    } type
        = SourceType::GLSL_FILE;

    ShaderSource() = default;
    ShaderSource(std::string content_, ShaderStage stage_, SourceType type_)
        : content(std::move(content_))
        , stage(stage_)
        , type(type_)
    {
    }
};

/**
 * @class ShaderFoundry
 * @brief Portal-level shader compilation and caching
 *
 * ShaderFoundry is a thin glue layer that:
 * - Wraps Core::VKShaderModule for convenient shader creation
 * - Provides caching to avoid redundant compilation
 * - Supports hot-reload workflows (watch files, recompile)
 * - Returns VKShaderModule directly for use in pipelines
 *
 * Design Philosophy:
 * - Manages compilation, NOT execution (that's Pipeline/Compute)
 * - Returns VKShaderModule directly (no wrapping)
 * - Simple, focused API aligned with VKShaderModule capabilities
 * - Integrates with existing Core shader infrastructure
 *
 * Consumers:
 * - VKBufferProcessor subclasses (compute shaders)
 * - Future Yantra::ComputePipeline (compute shaders)
 * - Future Yantra::RenderPipeline (graphics shaders)
 * - Future DataProcessors (image processing shaders)
 *
 * Usage:
 *   auto& compiler = Portal::Graphics::ShaderFoundry::instance();
 *
 *   // Compile from file
 *   auto shader = compiler.compile_from_file("shaders/my_kernel.comp");
 *
 *   // Compile from string
 *   auto shader = compiler.compile_from_source(glsl_code, ShaderStage::COMPUTE);
 *
 *   // Use in pipeline
 *   my_buffer_processor->set_shader(shader);
 *   my_compute_pipeline->set_shader(shader);
 */
class MAYAFLUX_API ShaderFoundry {
public:
    static ShaderFoundry& instance()
    {
        static ShaderFoundry compiler;
        return compiler;
    }

    ShaderFoundry(const ShaderFoundry&) = delete;
    ShaderFoundry& operator=(const ShaderFoundry&) = delete;
    ShaderFoundry(ShaderFoundry&&) noexcept = default;
    ShaderFoundry& operator=(ShaderFoundry&&) noexcept = default;

    /**
     * @brief Initialize shader compiler
     * @param backend Shared pointer to VulkanBackend
     * @param config Compiler configuration
     * @return True if initialization succeeded
     *
     * Must be called before compiling any shaders.
     */
    bool initialize(
        const std::shared_ptr<Core::VulkanBackend>& backend,
        const ShaderCompilerConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     *
     * Destroys all cached shader modules.
     * Safe to call multiple times.
     */
    void shutdown();

    /**
     * @brief Check if compiler is initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_backend != nullptr; }

    //==========================================================================
    // Shader Compilation - Primary API
    //==========================================================================

    /**
     * @brief Compile shader from file path
     * @param filepath Path to shader file (.comp, .vert, .frag, .spv, etc.)
     * @param stage Optional stage override (auto-detected from extension if omitted)
     * @param entry_point Entry point function name (default: "main")
     * @return Compiled shader module, or nullptr on failure
     *
     * Supports:
     * - GLSL files: .comp, .vert, .frag, .geom, .tesc, .tese
     * - SPIR-V files: .spv
     *
     * Stage auto-detection:
     *   .comp → COMPUTE
     *   .vert → VERTEX
     *   .frag → FRAGMENT
     *   .geom → GEOMETRY
     *   .tesc → TESS_CONTROL
     *   .tese → TESS_EVALUATION
     *
     * Caching: If the same file is compiled twice, returns cached version.
     * Use invalidate_cache() or hot_reload() to force recompilation.
     */
    std::shared_ptr<Core::VKShaderModule> compile_from_file(
        const std::string& filepath,
        std::optional<ShaderStage> stage = std::nullopt,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from GLSL source string
     * @param source GLSL source code
     * @param stage Shader stage (COMPUTE, VERTEX, FRAGMENT, etc.)
     * @param entry_point Entry point function name (default: "main")
     * @return Compiled shader module, or nullptr on failure
     *
     * Does NOT cache by default (source strings are transient).
     * Use compile_from_source_cached() if you want caching.
     */
    std::shared_ptr<Core::VKShaderModule> compile_from_source(
        const std::string& source,
        ShaderStage stage,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from GLSL source with caching
     * @param source GLSL source code
     * @param stage Shader stage
     * @param cache_key Unique key for caching (e.g., "my_kernel_v1")
     * @param entry_point Entry point function name
     * @return Compiled shader module, or nullptr on failure
     *
     * Caches compiled shader under cache_key.
     * Useful for generated shaders or frequently reused inline shaders.
     */
    std::shared_ptr<Core::VKShaderModule> compile_from_source_cached(
        const std::string& source,
        ShaderStage stage,
        const std::string& cache_key,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from SPIR-V file
     * @param spirv_path Path to .spv file
     * @param stage Shader stage (must specify, cannot auto-detect from SPIR-V)
     * @param entry_point Entry point function name (default: "main")
     * @return Compiled shader module, or nullptr on failure
     *
     * For pre-compiled SPIR-V binaries.
     * Caches by file path like compile_from_file().
     */
    std::shared_ptr<Core::VKShaderModule> compile_from_spirv(
        const std::string& spirv_path,
        ShaderStage stage,
        const std::string& entry_point = "main");

    /**
     * @brief Compile shader from ShaderSource descriptor
     * @param shader_source Shader descriptor (path or source + type + stage)
     * @return Compiled shader module, or nullptr on failure
     *
     * Unified interface that dispatches to appropriate compile method.
     * Useful for abstracted shader loading pipelines.
     */
    std::shared_ptr<Core::VKShaderModule> compile(const ShaderSource& shader_source);

    //==========================================================================
    // Hot-Reload Support
    //==========================================================================

    /**
     * @brief Invalidate cache for specific shader
     * @param cache_key File path or cache key
     *
     * Forces next compilation to recompile from source.
     * Useful for hot-reload workflows.
     */
    void invalidate_cache(const std::string& cache_key);

    /**
     * @brief Invalidate entire shader cache
     *
     * Forces all subsequent compilations to recompile.
     * Does NOT destroy existing shader modules (they remain valid).
     */
    void clear_cache();

    /**
     * @brief Hot-reload a shader from file
     * @param filepath Path to shader file
     * @return Recompiled shader module, or nullptr on failure
     *
     * Convenience method: invalidate_cache() + compile_from_file().
     * Returns new shader module; consumers must update references.
     */
    std::shared_ptr<Core::VKShaderModule> hot_reload(const std::string& filepath);

    //==========================================================================
    // Configuration
    //==========================================================================

    /**
     * @brief Update compiler configuration
     * @param config New configuration
     *
     * Affects future compilations.
     * Does NOT recompile existing shaders.
     */
    void set_config(const ShaderCompilerConfig& config);

    /**
     * @brief Get current compiler configuration
     */
    [[nodiscard]] const ShaderCompilerConfig& get_config() const { return m_config; }

    /**
     * @brief Add include directory for shader compilation
     * @param directory Path to directory containing shader includes
     *
     * Used for #include resolution in GLSL files.
     */
    void add_include_directory(const std::string& directory);

    /**
     * @brief Add preprocessor define for shader compilation
     * @param name Macro name
     * @param value Macro value (optional)
     *
     * Example: define("DEBUG", "1") → #define DEBUG 1
     */
    void add_define(const std::string& name, const std::string& value = "");

    //==========================================================================
    // Introspection
    //==========================================================================

    /**
     * @brief Check if shader is cached
     * @param cache_key File path or cache key
     */
    [[nodiscard]] bool is_cached(const std::string& cache_key) const;

    /**
     * @brief Get all cached shader keys
     */
    [[nodiscard]] std::vector<std::string> get_cached_keys() const;

    /**
     * @brief Get number of cached shaders
     */
    [[nodiscard]] size_t get_cache_size() const { return m_shader_cache.size(); }

    //==========================================================================
    // Utilities
    //==========================================================================

    /**
     * @brief Convert Portal ShaderStage to Vulkan ShaderStageFlagBits
     */
    static vk::ShaderStageFlagBits to_vulkan_stage(ShaderStage stage);

    /**
     * @brief Auto-detect shader stage from file extension
     * @param filepath Path to shader file
     * @return Detected stage, or nullopt if unknown
     *
     * Delegates to VKShaderModule::detect_stage_from_extension().
     */
    static std::optional<ShaderStage> detect_stage_from_extension(const std::string& filepath);

private:
    ShaderFoundry() = default;
    ~ShaderFoundry() { shutdown(); }

    std::shared_ptr<Core::VulkanBackend> m_backend;
    ShaderCompilerConfig m_config;

    // Shader cache (file path or cache key → shader module)
    std::unordered_map<std::string, std::shared_ptr<Core::VKShaderModule>> m_shader_cache;

    // Helper: Create shader module with current config
    std::shared_ptr<Core::VKShaderModule> create_shader_module();

    // Helper: Get device handle
    vk::Device get_device() const;
};

/**
 * @brief Get the global shader compiler instance
 * @return Reference to singleton shader compiler
 *
 * Must call initialize() before first use.
 * Thread-safe after initialization.
 */
inline MAYAFLUX_API ShaderFoundry& get_shader_compiler()
{
    return ShaderFoundry::instance();
}

} // namespace MayaFlux::Portal::Graphics
