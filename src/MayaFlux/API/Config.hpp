#include "MayaFlux/Journal/JournalEntry.hpp"
#include "MayaFlux/Nodes/NodeSpec.hpp"

namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    struct GlobalGraphicsConfig;
    struct GlobalInputConfig;
}

/**
 * @brief Checks if the default audio engine is initialized
 * @return True if the engine is initialized, false otherwise
 */
MAYAFLUX_API bool is_engine_initialized();

/**
@brief Globlal configuration for MayaFlux
*
* This namespace contains global configuration settings for MayaFlux, including graph and node configurations.
* It provides access to settings such as sample rate, buffer size, and number of output channels
* from the default audio engine.
*/
namespace Config {

    extern Nodes::NodeConfig node_config;

    MAYAFLUX_API Nodes::NodeConfig& get_node_config();

    /**
     * @brief Gets the sample rate from the default engine
     * @return Current sample rate in Hz
     */
    MAYAFLUX_API uint32_t get_sample_rate();

    /**
     * @brief Gets the buffer size from the default engine
     * @return Current buffer size in frames
     */
    MAYAFLUX_API uint32_t get_buffer_size();

    /**
     * @brief Gets the number of output channels from the default engine
     * @return Current number of output channels
     */
    MAYAFLUX_API uint32_t get_num_out_channels();

    /**
     * @brief Gets the stream configuration from the default engine
     * @return Copy of the GlobalStreamInfo struct
     */
    MAYAFLUX_API Core::GlobalStreamInfo& get_global_stream_info();

    /**
     * @brief Gets the graphics configuration from the default engine
     * @return Copy of the GlobalGraphicsConfig struct
     */
    MAYAFLUX_API Core::GlobalGraphicsConfig& get_global_graphics_config();

    /**
     * @brief Gets the input configuration from the default engine
     * @return Copy of the GlobalInputConfig struct
     */
    MAYAFLUX_API Core::GlobalInputConfig& get_global_input_config();

    /**
     * @brief Sets the minimum severity level for journal entries to be logged
     * @param severity Minimum severity level (e.g., INFO, WARN, ERROR, TRACE, DEBUG, FATAL)
     */
    MAYAFLUX_API void set_journal_severity(Journal::Severity severity);

    /**
     * @brief Stores journal entries to a file by adding a FileSink to the Archivist
     * @param file_name Path to the log file
     */
    MAYAFLUX_API void store_journal_entries(const std::string& file_name);

    /**
     * @brief Outputs journal entries to the console by adding a ConsoleSink to the Archivist
     * NOTE: This records thread safe entries and cannot unsink once called.
     */
    MAYAFLUX_API void sink_journal_to_console();
}

}
