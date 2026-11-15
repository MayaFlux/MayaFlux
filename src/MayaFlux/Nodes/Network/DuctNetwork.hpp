#pragma once

#include "NodeNetwork.hpp"

#include "MayaFlux/Kinesis/Acoustics/DuctGrid.hpp"
#include "MayaFlux/Kinesis/Acoustics/DuctLoss.hpp"
#include "Operators/TerminationOperators.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class DuctNetwork
 * @brief A one dimensional acoustic duct of any bore shape, solved by finite
 *        differences, with a termination at each end.
 *
 * Pressure and volume velocity sit on an interleaved grid along the duct, one
 * sample per step, with the cross section read from a radius profile. A
 * cylinder, a cone and any other shape differ only in the profile. Each end is
 * a TerminationOperator: by default a ReedOperator at the mouth, so the duct
 * sounds when blown, and a RadiationOperator at the bell, so it radiates. A
 * WallOperator closes an end, and a JunctionOperator takes any pressure
 * dependent flow.
 *
 * ```cpp
 * auto duct = std::make_shared<DuctNetwork>(0.6, 0.0075) | Audio[0];
 * duct->map_parameter("pressure", breath);
 * duct->set_output_scale(100.0);
 * ```
 *
 * The output is the pressure radiated at one metre in pascals, or the
 * pressure at a pickup position. Pascals are small as audio, so scale it with
 * set_output_scale.
 *
 * Mapped parameters (BROADCAST): "pickup" (0 to 1 along the duct), "scale",
 * and anything else is offered to both terminations, so "pressure",
 * "closing_pressure" and "opening" reach the reed.
 *
 * Changing the length, the profile or a termination builds the new grid on the
 * calling thread and installs it at a block boundary without waiting, so none
 * of that touches the audio path. The state restarts from rest when it is
 * installed. These calls come from one thread at a time, not from the audio
 * thread, and return false, leaving the duct as it was, when the new grid or a
 * termination cannot be built.
 *
 * The sample rate is fixed at construction and sets the grid.
 */
class MAYAFLUX_API DuctNetwork final : public NodeNetwork {
public:
    /** @brief What the network publishes. */
    enum class Output : uint8_t {
        RADIATED, ///< Pressure radiated at one metre, from the flow out of the bell
        PICKUP ///< Pressure inside the duct at the pickup position
    };

    /**
     * @brief A cylinder.
     * @param length Length in metres
     * @param radius Radius in metres
     * @param sample_rate Sample rate in Hz
     */
    DuctNetwork(double length, double radius, double sample_rate = 48000.0);

    /**
     * @brief A duct from radii sampled evenly along its length.
     * @param length Length in metres
     * @param radii At least two radii in metres, from the mouth to the bell
     * @param sample_rate Sample rate in Hz
     */
    DuctNetwork(double length, const std::vector<double>& radii, double sample_rate = 48000.0);

    /**
     * @brief A duct from a radius function.
     * @param length Length in metres
     * @param radius Callable giving the radius in metres at a distance in
     *        metres from the mouth
     * @param sample_rate Sample rate in Hz
     */
    DuctNetwork(double length, std::function<double(double)> radius, double sample_rate = 48000.0);

    ~DuctNetwork() override;

    DuctNetwork(const DuctNetwork&) = delete;
    DuctNetwork& operator=(const DuctNetwork&) = delete;
    DuctNetwork(DuctNetwork&&) = delete;
    DuctNetwork& operator=(DuctNetwork&&) = delete;

    //-------------------------------------------------------------------------
    // NodeNetwork Interface
    //-------------------------------------------------------------------------

    void process_batch(unsigned int num_samples) override;

    /** @brief Number of pressure points along the duct. */
    [[nodiscard]] size_t get_node_count() const override { return m_field_points.load(std::memory_order_relaxed); }

    void initialize() override;

    /** @brief Return the duct to rest, installed at the next block. */
    void reset() override;

    /** @brief Pressure at one point along the duct, from the last block. */
    [[nodiscard]] std::optional<double> get_node_output(size_t index) const override;
    [[nodiscard]] std::optional<std::span<const double>> get_node_audio_buffer(size_t index) const override;
    [[nodiscard]] std::unordered_map<std::string, std::string> get_metadata() const override;

    void map_parameter(const std::string& param_name,
        const std::shared_ptr<Node>& source,
        MappingMode mode = MappingMode::BROADCAST) override;

    void map_parameter(const std::string& param_name,
        const std::shared_ptr<NodeNetwork>& source) override;

    void unmap_parameter(const std::string& param_name) override;

    //-------------------------------------------------------------------------
    // Shape
    //-------------------------------------------------------------------------

    /** @brief Change the length in metres, keeping the profile. */
    [[nodiscard]] bool set_length(double length);

    /** @brief Make the duct a cylinder of this radius in metres. */
    [[nodiscard]] bool set_radius(double radius);

    /** @brief Replace the profile with radii sampled evenly from mouth to bell. */
    [[nodiscard]] bool set_profile(const std::vector<double>& radii);

    /** @brief Replace the profile with a radius function of distance from the mouth. */
    [[nodiscard]] bool set_profile(std::function<double(double)> radius);

    /**
     * @brief Add air behind the mouth end, in cubic metres.
     * @return False, changing nothing, for a volume that is negative or not finite
     *
     * A mouthpiece cavity, or the tip a truncated cone lacks: pi r^2 x / 3 for
     * apex radius r and distance x to the apex. It lets a cone keep its whole
     * number series of modes.
     */
    [[nodiscard]] bool set_mouth_volume(double volume);

    //-------------------------------------------------------------------------
    // Wall losses
    //-------------------------------------------------------------------------

    /**
     * @brief Set the wall losses by hand.
     * @param friction Friction in 1/s, not negative
     * @param viscosity Viscosity in m^2/s, not negative
     * @return False, changing nothing, for a rate that is negative or not finite
     *
     * Zero for both gives the lossless duct. The default is
     * use_viscothermal_losses.
     */
    [[nodiscard]] bool set_wall_losses(double friction, double viscosity);

    /**
     * @brief Take the wall losses from the duct's mean radius, fitted to the
     *        viscothermal attenuation of a tube at 150 Hz and 3 kHz.
     */
    [[nodiscard]] bool use_viscothermal_losses();

    //-------------------------------------------------------------------------
    // Terminations
    //-------------------------------------------------------------------------

    /**
     * @brief Replace the termination at the mouth.
     * @return False, changing nothing, when it cannot terminate this duct
     */
    [[nodiscard]] bool set_mouth(std::shared_ptr<TerminationOperator> termination);

    /** @brief Replace the termination at the bell. */
    [[nodiscard]] bool set_bell(std::shared_ptr<TerminationOperator> termination);

    [[nodiscard]] std::shared_ptr<TerminationOperator> get_mouth() const;
    [[nodiscard]] std::shared_ptr<TerminationOperator> get_bell() const;

    //-------------------------------------------------------------------------
    // Output
    //-------------------------------------------------------------------------

    void set_output(Output output) { m_output.store(output, std::memory_order_relaxed); }
    [[nodiscard]] Output get_output() const { return m_output.load(std::memory_order_relaxed); }

    /** @brief Pickup position as a fraction of the length, clamped to 0 to 1. */
    void set_pickup_position(double position);
    [[nodiscard]] double get_pickup_position() const { return m_pickup.load(std::memory_order_relaxed); }

    /** @brief Copy of the pressures along the duct from the last block. */
    [[nodiscard]] std::vector<double> pressure_field() const;

private:
    struct Prepared;

    struct Profile {
        std::vector<double> radii;
        std::function<double(double)> function;
    };

    struct Losses {
        bool automatic { true };
        double friction {};
        double viscosity {};
    };

    void setup(double length, Profile profile, double sample_rate);

    [[nodiscard]] std::shared_ptr<Prepared> build(
        double length,
        const Profile& profile,
        const Losses& losses,
        double mouth_volume,
        const std::shared_ptr<TerminationOperator>& mouth,
        const std::shared_ptr<TerminationOperator>& bell) const;

    void publish(std::shared_ptr<Prepared> next);
    void install_pending();
    void bind(Prepared& prepared);
    void update_mapped_parameters();
    void publish_block(std::span<const double> block);

    mutable std::mutex m_producer_mutex;
    double m_length {};
    double m_rate {};
    Profile m_profile;
    Losses m_losses;
    double m_mouth_volume {};
    double m_applied_friction {};
    double m_applied_viscosity {};
    std::shared_ptr<TerminationOperator> m_mouth;
    std::shared_ptr<TerminationOperator> m_bell;
    size_t m_intervals {};
    double m_courant {};
    double m_bandwidth {};

    std::shared_ptr<Prepared> m_active;
    std::shared_ptr<Prepared> m_slot;
    std::atomic<uint8_t> m_handoff { 0 };

    std::atomic<Output> m_output { Output::RADIATED };
    std::atomic<double> m_pickup { 0.5 };
    std::atomic<size_t> m_field_points { 0 };

    std::vector<double> m_scratch;
    std::vector<double> m_field;
    double m_previous_outflow {};
    double m_last_output {};
};

} // namespace MayaFlux::Nodes::Network
