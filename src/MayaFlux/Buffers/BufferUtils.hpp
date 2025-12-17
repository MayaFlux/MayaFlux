#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

namespace MayaFlux::Nodes {
class Node;
}

namespace MayaFlux::Buffers {

/**
 * @enum TokenEnforcementStrategy
 * @brief Defines how strictly processing token requirements are enforced in buffer processing chains
 *
 * TokenEnforcementStrategy provides different levels of flexibility for handling processor-buffer
 * compatibility based on processing tokens. This allows the system to balance performance optimization
 * with operational flexibility depending on the application's requirements.
 *
 * The enforcement strategy affects how BufferProcessingChain handles processors with incompatible
 * tokens, ranging from strict validation to complete flexibility. This enables different operational
 * modes for development, production, and specialized processing scenarios.
 */
enum class TokenEnforcementStrategy : uint8_t {
    /**
     * @brief Strictly enforces token assignment with no cross-token sharing
     *
     * Processors must exactly match the buffer's processing token requirements.
     * Any incompatibility results in immediate rejection. This provides maximum
     * performance optimization by ensuring all processors in a chain can execute
     * with the same backend configuration, but offers the least flexibility.
     */
    STRICT,

    /**
     * @brief Filters processors through token enumeration, allowing compatible combinations
     *
     * Uses the are_tokens_compatible() function to determine if processors can work
     * together despite different token assignments. This allows some flexibility while
     * maintaining performance optimization for compatible processor combinations.
     * Incompatible processors are filtered out rather than rejected outright.
     */
    FILTERED,

    /**
     * @brief Allows token overrides but skips processing for incompatible operations
     *
     * Permits processors with different tokens to be added to processing chains,
     * but skips their execution when the tokens are incompatible. This maintains
     * chain integrity while allowing dynamic processor management. Useful for
     * conditional processing scenarios where not all processors need to execute.
     */
    OVERRIDE_SKIP,

    /**
     * @brief Allows token overrides but rejects incompatible processors from chains
     *
     * Similar to OVERRIDE_SKIP but removes incompatible processors from the chain
     * entirely rather than skipping them. This provides a middle ground between
     * flexibility and performance by cleaning up incompatible processors while
     * allowing initial token mismatches during chain construction.
     */
    OVERRIDE_REJECT,

    /**
     * @brief Ignores token assignments completely, allowing any processing combination
     *
     * Disables all token validation and compatibility checking. Any processor can
     * be added to any buffer's processing chain regardless of token compatibility.
     * This provides maximum flexibility but may result in suboptimal performance
     * or execution errors. Primarily useful for debugging or specialized scenarios.
     */
    IGNORE
};

/**
 * @brief Validates that a processing token has a valid, non-conflicting configuration
 * @param token Processing token to validate
 * @throws std::invalid_argument if the token contains mutually exclusive flags
 *
 * This function ensures that processing tokens contain only compatible flag combinations.
 * It validates three key mutual exclusions that are fundamental to the processing model:
 *
 * **Rate Mutual Exclusion**: SAMPLE_RATE and FRAME_RATE cannot be combined as they
 * represent fundamentally different temporal processing models that cannot be executed
 * simultaneously within the same processing context.
 *
 * **Device Mutual Exclusion**: CPU_PROCESS and GPU_PROCESS cannot be combined as they
 * represent different execution environments that require different resource allocation
 * and execution strategies.
 *
 * **Concurrency Mutual Exclusion**: SEQUENTIAL and PARALLEL cannot be combined as they
 * represent incompatible execution patterns that would create undefined behavior in
 * processing chains.
 *
 * This validation is essential for maintaining system stability and ensuring that
 * processing tokens represent achievable execution configurations.
 */
void validate_token(ProcessingToken token);

/**
 * @brief Determines if two processing tokens are compatible for joint execution
 * @param preferred The preferred processing token configuration
 * @param current The current processing token configuration being evaluated
 * @return true if the tokens are compatible, false otherwise
 *
 * This function implements sophisticated compatibility logic that goes beyond simple equality
 * checking to determine if processors with different token requirements can work together
 * in the same processing pipeline. The compatibility rules are designed to maximize
 * processing flexibility while maintaining system stability and performance.
 *
 * **Rate Compatibility Rules:**
 * - FRAME_RATE processors require FRAME_RATE execution contexts (strict requirement)
 * - SAMPLE_RATE processors can adapt to FRAME_RATE contexts (flexible upward compatibility)
 * - Same-rate combinations are always compatible
 *
 * **Device Compatibility Rules:**
 * - SAMPLE_RATE processing cannot execute on GPU hardware (hardware limitation)
 * - GPU-preferred processors cannot fall back to CPU execution (performance requirement)
 * - CPU-preferred processors can use GPU for FRAME_RATE processing only
 *
 * **Concurrency Compatibility Rules:**
 * - Sequential/Parallel differences are acceptable if rate requirements align
 * - Mismatched concurrency with incompatible rates is rejected
 * - Same concurrency patterns are always compatible
 *
 * This flexibility enables the system to optimize processing chains by allowing compatible
 * processors to share execution contexts while preventing configurations that would result
 * in poor performance or execution failures.
 */
bool are_tokens_compatible(ProcessingToken preferred, ProcessingToken current);

/**
 * @brief Gets the optimal processing token for a given buffer type and system configuration
 * @param buffer_type Type identifier for the buffer (e.g., "audio", "video", "texture")
 * @param system_capabilities Available system capabilities (GPU, multi-core CPU, etc.)
 * @return Recommended processing token for optimal performance
 *
 * This function analyzes buffer characteristics and system capabilities to recommend
 * the most appropriate processing token configuration. It considers factors like:
 * - Buffer data type and size characteristics
 * - Available hardware acceleration
 * - System performance characteristics
 * - Current system load and resource availability
 *
 * The recommendations help achieve optimal performance by matching processing
 * requirements with available system capabilities.
 */
ProcessingToken get_optimal_token(const std::string& buffer_type, uint32_t system_capabilities);

constexpr int MAX_SPINS = 1000;

/**
 * @brief Wait for an active snapshot context to complete using exponential backoff
 * @return true if completed, false if timeout
 */
bool wait_for_snapshot_completion(
    const std::shared_ptr<Nodes::Node>& node,
    uint64_t active_context_id,
    int max_spins = MAX_SPINS);

/**
 * @brief Extract a single sample from a node with proper snapshot management
 * @return Extracted sample value
 */
double extract_single_sample(const std::shared_ptr<Nodes::Node>& node);

/**
 * @brief Extract multiple samples from a node into a vector
 */
std::vector<double> extract_multiple_samples(
    const std::shared_ptr<Nodes::Node>& node,
    size_t num_samples);

/**
 * @brief Apply node output to an existing buffer with mixing
 */
void update_buffer_with_node_data(
    const std::shared_ptr<Nodes::Node>& node,
    std::span<double> buffer,
    double mix = 1.0);

} // namespace MayaFlux::Buffers

namespace std {
template <>
struct hash<std::pair<MayaFlux::Buffers::ProcessingToken, MayaFlux::Buffers::ProcessingToken>> {
    size_t operator()(const std::pair<MayaFlux::Buffers::ProcessingToken, MayaFlux::Buffers::ProcessingToken>& pair) const
    {
        return hash<uint32_t>()(static_cast<uint32_t>(pair.first)) ^ (hash<uint32_t>()(static_cast<uint32_t>(pair.second)) << 1);
    }
};
}
