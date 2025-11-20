#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes {

/**
 * @class NodeNetwork
 * @brief Abstract base class for structured collections of nodes with defined
 * relationships
 *
 * DESIGN PRINCIPLES:
 * =================
 *
 * 1. OWNERSHIP: Networks own their nodes exclusively. Nodes within a network
 *    cannot be independently attached to NodeGraphManager channels.
 *
 * 2. PROCESSING: Networks are processed directly by NodeGraphManager, parallel
 *    to RootNodes. They are NOT summed through RootNode but manage their own
 *    internal processing pipeline.
 *
 * 3. OUTPUT ROUTING: Networks explicitly declare their output mode:
 *    - NONE: Pure internal computation (e.g., particle physics state)
 *    - AUDIO_SINK: Contributes to audio output (aggregated samples)
 *    - GRAPHICS_BIND: Data available for visualization (read-only)
 *    - CUSTOM: User-defined handling via callbacks
 *
 * 4. TOPOLOGY: Networks define relationships between nodes:
 *    - INDEPENDENT: No inter-node connections
 *    - CHAIN: Linear sequence (node[i] → node[i+1])
 *    - RING: Circular (last connects to first)
 *    - GRID_2D/3D: Spatial lattice with neighbor connections
 *    - SPATIAL: Dynamic proximity-based relationships
 *    - CUSTOM: Arbitrary user-defined topology
 *
 * 5. EXTENSIBILITY: Subclasses define:
 *    - Internal node data structure (spatial, physical, abstract)
 *    - Interaction behavior (forces, coupling, feedback)
 *    - Aggregation logic (how to combine node outputs)
 *    - Initialization patterns (how to populate the network)
 *
 * PHILOSOPHY:
 * ===========
 *
 * NodeNetworks embody "structure IS content" - the relationships between
 * nodes define emergent behavior. They bridge individual node computation
 * with collective, coordinated behavior patterns (swarms, resonances,
 * waveguides, recursive growth).
 *
 * Networks are NOT:
 * - Buffers (no sequential data storage)
 * - Processors (no transformation pipelines)
 * - Simple node containers (relationships matter)
 *
 * Networks ARE:
 * - Relational structures for coordinated node behavior
 * - Generators of emergent complexity from simple rules
 * - Cross-domain abstractions (audio, visual, control unified)
 *
 * USAGE PATTERN:
 * ==============
 *
 * ```cpp
 * // Create network via builder or subclass constructor
 * auto particles = std::make_shared<ParticleNetwork>(1000);
 * particles->set_output_mode(OutputMode::GRAPHICS_BIND);
 * particles->initialize_random_positions();
 *
 * // Register with NodeGraphManager (NOT RootNode)
 * node_graph_manager->add_network(particles, ProcessingToken::VISUAL_RATE);
 *
 * // NodeGraphManager calls process_batch() each frame
 * // Graphics nodes can read network state for visualization
 * auto geom = std::make_shared<NetworkGeometryNode>(particles);
 * ```
 */
class MAYAFLUX_API NodeNetwork {
public:
  /**
   * @enum Topology
   * @brief Defines the structural relationships between nodes in the network
   */
  enum class Topology : uint8_t {
    INDEPENDENT, ///< No connections, nodes process independently
    CHAIN,       ///< Linear sequence: node[i] → node[i+1]
    RING,        ///< Circular: last node connects to first
    GRID_2D,     ///< 2D lattice with 4-connectivity
    GRID_3D,     ///< 3D lattice with 6-connectivity
    SPATIAL,     ///< Dynamic proximity-based (nodes within radius interact)
    CUSTOM       ///< User-defined arbitrary topology
  };

  /**
   * @enum OutputMode
   * @brief Defines how the network's computational results are exposed
   */
  enum class OutputMode : uint8_t {
    NONE,          ///< Pure internal state, no external output
    AUDIO_SINK,    ///< Aggregated audio samples sent to output
    GRAPHICS_BIND, ///< State available for visualization (read-only)
    CUSTOM         ///< User-defined output handling via callbacks
  };

  /**
   * @enum MappingMode
   * @brief Defines how nodes map to external entities (e.g., audio channels,
   * graphics objects)
   */
  enum class MappingMode : uint8_t {
    BROADCAST, ///< One node → all network nodes
    ONE_TO_ONE ///< Node array/network → network nodes (must match count)
  };

  virtual ~NodeNetwork() = default;

  //-------------------------------------------------------------------------
  // Core Abstract Interface (MUST be implemented by subclasses)
  //-------------------------------------------------------------------------

  /**
   * @brief Process the network for the given number of samples
   * @param num_samples Number of samples/frames to process
   *
   * Subclasses implement their specific processing logic:
   * 1. Update internal state (physics, relationships, etc.)
   * 2. Process individual nodes
   * 3. Apply inter-node interactions
   * 4. Aggregate outputs if needed
   *
   * Called by NodeGraphManager during token processing.
   */
  virtual void process_batch(unsigned int num_samples) = 0;

  /**
   * @brief Get the number of nodes in the network
   * @return Total node count
   *
   * Used for introspection, visualization, and validation.
   */
  [[nodiscard]] virtual size_t get_node_count() const = 0;

  //-------------------------------------------------------------------------
  // Output Interface (Default implementations provided)
  //-------------------------------------------------------------------------

  /**
   * @brief Get aggregated audio output (if OutputMode::AUDIO_SINK)
   * @return Optional audio sample value
   *
   * Default implementation returns std::nullopt.
   * Override if network contributes to audio output.
   *
   * Aggregation strategy is subclass-defined:
   * - Sum all node outputs
   * - Average node outputs
   * - Select specific nodes
   * - Apply custom mixing
   */
  [[nodiscard]] virtual std::optional<double> get_audio_output() const {
    return std::nullopt;
  }

  /**
   * @brief Get multi-channel audio output
   * @return Optional vector of per-channel samples
   *
   * Override for networks that produce multi-channel audio.
   * Default returns std::nullopt.
   */
  [[nodiscard]] virtual std::optional<std::vector<double>>
  get_audio_outputs() const {
    return std::nullopt;
  }

  //-------------------------------------------------------------------------
  // Configuration (Non-virtual, base class managed)
  //-------------------------------------------------------------------------

  /**
   * @brief Set the network's output routing mode
   */
  void set_output_mode(OutputMode mode) { m_output_mode = mode; }

  /**
   * @brief Get the current output routing mode
   */
  [[nodiscard]] OutputMode get_output_mode() const { return m_output_mode; }

  /**
   * @brief Set the network's topology
   */
  void set_topology(Topology topology) { m_topology = topology; }

  /**
   * @brief Get the current topology
   */
  [[nodiscard]] Topology get_topology() const { return m_topology; }

  /**
   * @brief Enable/disable the network
   *
   * Disabled networks skip processing but maintain state.
   */
  void set_enabled(bool enabled) { m_enabled = enabled; }

  /**
   * @brief Check if network is enabled
   */
  [[nodiscard]] bool is_enabled() const { return m_enabled; }

  //-------------------------------------------------------------------------
  // Introspection (Optional overrides for subclass-specific data)
  //-------------------------------------------------------------------------

  /**
   * @brief Get network metadata for debugging/visualization
   * @return Map of property names to string representations
   *
   * Subclasses can override to expose internal state:
   * - Particle count, average velocity
   * - Modal frequencies, decay times
   * - Waveguide delay lengths
   * etc.
   */
  [[nodiscard]] virtual std::unordered_map<std::string, std::string>
  get_metadata() const {
    return {{"topology", topology_to_string(m_topology)},
            {"output_mode", output_mode_to_string(m_output_mode)},
            {"node_count", std::to_string(get_node_count())},
            {"enabled", m_enabled ? "true" : "false"}};
  }

  //-------------------------------------------------------------------------
  // Lifecycle Hooks (Optional overrides)
  //-------------------------------------------------------------------------

  /**
   * @brief Called once before first process_batch()
   *
   * Use for expensive one-time initialization:
   * - Building neighbor maps
   * - Allocating buffers
   * - Computing lookup tables
   */
  virtual void initialize() {}

  /**
   * @brief Reset network to initial state
   *
   * Override to implement network-specific reset logic:
   * - Clear particle velocities
   * - Reset modal phases
   * - Rebuild topology
   */
  virtual void reset() {}

  //-------------------------------------------------------------------------
  // Mapping Hooks
  //-------------------------------------------------------------------------

  /**
   * @brief Map external node output to network parameter
   * @param param_name Parameter name (network-specific, e.g., "brightness",
   * "frequency")
   * @param source Single node for BROADCAST
   * @param mode Mapping mode
   * @note Default implementation stores mapping; subclasses handle in
   * process_batch(). This methoud SHOULD BE OVERRIDDEN by child classes that
   * need to handle parameter mappings.
   */
  virtual void map_parameter(const std::string &param_name,
                             const std::shared_ptr<Node> &source,
                             MappingMode mode = MappingMode::BROADCAST) {
    // Default: store mapping, subclass handles in process_batch()
    m_parameter_mappings.push_back({param_name, mode, source, nullptr});
  }
  /**
   * @brief Map external node network to network parameters (ONE_TO_ONE)
   * @param param_name Parameter name
   * @param source_network NodeNetwork with matching node count
   * @note Default implementation stores mapping; subclasses handle in
   * process_batch(). This methoud SHOULD BE OVERRIDDEN by child classes that
   * need to handle parameter mappings.
   */
  virtual void
  map_parameter(const std::string &param_name,
                const std::shared_ptr<NodeNetwork> &source_network) {
    m_parameter_mappings.push_back(
        {param_name, MappingMode::ONE_TO_ONE, nullptr, source_network});
  }
  /**
   * @brief Remove parameter mapping
   */
  virtual void unmap_parameter(const std::string &param_name) {
    m_parameter_mappings.erase(
        std::remove_if(
            m_parameter_mappings.begin(), m_parameter_mappings.end(),
            [&](const auto &m) { return m.param_name == param_name; }),
        m_parameter_mappings.end());
  }

protected:
  //-------------------------------------------------------------------------
  // Protected State (Accessible to subclasses)
  //-------------------------------------------------------------------------

  Topology m_topology = Topology::INDEPENDENT;
  OutputMode m_output_mode = OutputMode::NONE;
  bool m_enabled = true;
  bool m_initialized = false;

  /**
   * @brief Ensure initialize() is called exactly once
   */
  void ensure_initialized() {
    if (!m_initialized) {
      initialize();
      m_initialized = true;
    }
  }

  //-------------------------------------------------------------------------
  // Utility Functions (Helper methods for subclasses)
  //-------------------------------------------------------------------------

  /**
   * @brief Build neighbor map for GRID_2D topology
   * @param width Grid width
   * @param height Grid height
   * @return Map of node index to neighbor indices (4-connectivity)
   */
  static std::unordered_map<size_t, std::vector<size_t>>
  build_grid_2d_neighbors(size_t width, size_t height);

  /**
   * @brief Build neighbor map for GRID_3D topology
   * @param width Grid width
   * @param height Grid height
   * @param depth Grid depth
   * @return Map of node index to neighbor indices (6-connectivity)
   */
  static std::unordered_map<size_t, std::vector<size_t>>
  build_grid_3d_neighbors(size_t width, size_t height, size_t depth);

  /**
   * @brief Build neighbor map for RING topology
   * @param count Total node count
   * @return Map of node index to [prev, next] indices
   */
  static std::unordered_map<size_t, std::vector<size_t>>
  build_ring_neighbors(size_t count);

  /**
   * @brief Build neighbor map for CHAIN topology
   * @param count Total node count
   * @return Map of node index to [next] or [prev] index
   */
  static std::unordered_map<size_t, std::vector<size_t>>
  build_chain_neighbors(size_t count);

  struct ParameterMapping {
    std::string param_name;
    MappingMode mode;

    // Storage for mapped sources
    std::shared_ptr<Node> broadcast_source;      // For BROADCAST
    std::shared_ptr<NodeNetwork> network_source; // For ONE_TO_ONE
  };

  std::vector<ParameterMapping> m_parameter_mappings;

private:
  //-------------------------------------------------------------------------
  // String Conversion (For metadata/debugging)
  //-------------------------------------------------------------------------

  static std::string topology_to_string(Topology topo) {
    switch (topo) {
    case Topology::INDEPENDENT:
      return "INDEPENDENT";
    case Topology::CHAIN:
      return "CHAIN";
    case Topology::RING:
      return "RING";
    case Topology::GRID_2D:
      return "GRID_2D";
    case Topology::GRID_3D:
      return "GRID_3D";
    case Topology::SPATIAL:
      return "SPATIAL";
    case Topology::CUSTOM:
      return "CUSTOM";
    default:
      return "UNKNOWN";
    }
  }

  static std::string output_mode_to_string(OutputMode mode) {
    switch (mode) {
    case OutputMode::NONE:
      return "NONE";
    case OutputMode::AUDIO_SINK:
      return "AUDIO_SINK";
    case OutputMode::GRAPHICS_BIND:
      return "GRAPHICS_BIND";
    case OutputMode::CUSTOM:
      return "CUSTOM";
    default:
      return "UNKNOWN";
    }
  }
};

} // namespace MayaFlux::Nodes
