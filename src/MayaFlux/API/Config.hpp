#include "MayaFlux/Utils.hpp"

namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    struct GlobalGraphicsConfig;
}

/**
@brief Globlal configuration for MayaFlux
*
* This namespace contains global configuration settings for MayaFlux, including graph and node configurations.
* It provides access to settings such as sample rate, buffer size, and number of output channels
* from the default audio engine.
*/
namespace Config {
    /**
     * @brief Configuration settings for the audio graph
     */
    struct GraphConfig {
        Utils::NodeChainSemantics chain_semantics;
        Utils::NodeBinaryOpSemantics binary_op_semantics;

        GraphConfig();
    };

    /**
     * @brief Configuration settings for individual audio nodes
     */
    struct NodeConfig {
        size_t channel_cache_size = 256; ///< Number of cached channels for oprations
        uint32_t max_channels = 32; ///< Maximum number of channels supported (uint32_t bits)
        size_t callback_cache_size = 64;
        size_t timer_cleanup_threshold = 20;
    };

    extern GraphConfig graph_config;
    extern NodeConfig node_config;

    GraphConfig& get_graph_config();
    NodeConfig& get_node_config();

    /**
     * @brief Gets the sample rate from the default engine
     * @return Current sample rate in Hz
     */
    uint32_t get_sample_rate();

    /**
     * @brief Gets the buffer size from the default engine
     * @return Current buffer size in frames
     */
    uint32_t get_buffer_size();

    /**
     * @brief Gets the number of output channels from the default engine
     * @return Current number of output channels
     */
    uint32_t get_num_out_channels();

    /**
     * @brief Gets the stream configuration from the default engine
     * @return Copy of the GlobalStreamInfo struct
     */
    Core::GlobalStreamInfo& get_global_stream_info();

    /**
     * @brief Gets the graphics configuration from the default engine
     * @return Copy of the GlobalGraphicsConfig struct
     */
    Core::GlobalGraphicsConfig& get_global_graphics_config();

    // WindowContext& window_context;
}

}
