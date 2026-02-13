#pragma once

namespace MayaFlux::Nodes {

/**
 * @enum NodeChainSemantics
 * @brief Defines how to handle existing nodes when creating a new chain
 */
enum NodeChainSemantics : uint8_t {
    REPLACE_TARGET, ///< Unregister the target and register with the new chain node
    PRESERVE_BOTH, ///< Preserve both nodes in the chain, add new chain node to root, i.e doubling the target signal
    ONLY_CHAIN ///< Only keep the new chain node, unregistering the source and target
};

/**
 * @enum NodeMixSemantics
 * @brief Defines how to handle existing nodes when creating a new mix
 */
enum NodeBinaryOpSemantics : uint8_t {
    REPLACE, ///< Unregister both nodes and register with the new binary op node
    KEEP ///< Preserve both nodes in the binary op, add new binary op node to root, i.e doubling the signal
};

/**
 * @brief Configuration settings for individual audio nodes
 */
struct NodeConfig {
    size_t channel_cache_size { 256 }; ///< Number of cached channels for oprations
    uint32_t max_channels { 32 }; ///< Maximum number of channels supported (uint32_t bits)
    size_t callback_cache_size { 64 };
    size_t timer_cleanup_threshold { 20 };

    NodeChainSemantics chain_semantics { NodeChainSemantics::REPLACE_TARGET };
    NodeBinaryOpSemantics binary_op_semantics { NodeBinaryOpSemantics::REPLACE };
};

/**
 * @enum NodeState
 * @brief Represents the processing state of a node in the audio graph
 */
enum NodeState : uint32_t {
    INACTIVE = 0x00, ///< Engine is not processing this node
    ACTIVE = 0x01, ///< Engine is processing this node
    PENDING_REMOVAL = 0x02, ///< Node is marked for removal

    MOCK_PROCESS = 0x04, ///< Node should be processed but output ignored
    PROCESSED = 0x08, ///< Node has been processed this cycle

    ENGINE_PROCESSED = ACTIVE | PROCESSED, ///< Engine has processed this node
    EXTERMAL_PROCESSED = INACTIVE | PROCESSED, ///< External source has processed this node
    ENGINE_MOCK_PROCESSED = ACTIVE | MOCK_PROCESS | PROCESSED, ///< Engine has mock processed this node
};

/**
 * @struct RoutingState
 * @brief Represents the state of routing transitions for a node
 *
 * This structure tracks the state of routing changes, including fade-in and fade-out phases,
 * channel counts, and elapsed cycles. It's used to manage smooth transitions when routing
 * changes occur, ensuring seamless audio output during dynamic reconfigurations of the processing graph.
 */
struct RoutingState {
    double amount[32];
    uint32_t cycles_elapsed {};
    uint32_t from_channels {};
    uint32_t to_channels {};
    uint32_t fade_cycles {};

    /**
     * @enum Phase
     * @brief Represents the current phase of a routing transition
     *
     * This enum defines the different phases of a routing transition, such as fade-out and fade-in.
     * It allows the audio engine to manage the transition process effectively, ensuring that audio
     * output remains seamless during dynamic changes in the processing graph.
     */
    enum Phase : uint8_t {
        NONE = 0x00, ///< No routing transition is currently active
        ACTIVE = 0x01, ///< Currently in the fade-out phase of a routing transition
    } phase { Phase::NONE };
};

}
