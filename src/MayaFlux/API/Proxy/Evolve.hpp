#pragma once

#include "MayaFlux/Transitive/Memory/LiveArena.hpp"

#include "MayaFlux/Buffers/State/RelaxationGridBuffer.hpp"
#include "MayaFlux/Buffers/State/VolumeGridBuffer.hpp"

namespace MayaFlux::Nodes::Network {
class NodeNetwork;
class GpuFieldOperator;
struct SpatialFieldConfig;
}

namespace MayaFlux {

/**
 * @concept GridCellData
 * @brief Cell types a relaxation grid can be seeded with from a host callable.
 *
 * Arithmetic scalars, two component vectors and four component vectors whose
 * size is a multiple of four bytes, so sizeof matches the std430 array stride
 * the rule and emit shaders read. Three component vectors are excluded:
 * glm::vec3 is 12 bytes but strides 16 in an SSBO array, so a rule with such a
 * cell declares a four component format instead.
 */
template <typename State>
concept GridCellData = (ArithmeticData<State> || GlmVec2Type<State> || GlmVec4Type<State>)
    && (sizeof(State) % sizeof(uint32_t) == 0);

/**
 * @class Evolve
 * @brief Sets a system going from a config: state that persists, a rule that
 *        advances it every cycle, and rendering attached.
 *
 * Owned by Creator as a member. Each call performs the whole sequence its
 * arguments imply and returns the real object it built or was handed, so
 * nothing is left to attach afterwards. The returned object carries its own
 * processors and advances on its own each cycle. The call sets it going and
 * does not step it.
 *
 * An evolving system is not a type here. It is a config handed to a class,
 * and the sequence that class needs is what this performs. The name says
 * nothing about a medium: it covers whatever the config's class advances,
 * which today is three families, selected by the config they consume:
 * - RelaxationGridBuffer::GridConfig builds a grid, seeds it, registers it in
 *   the graphics domain and attaches rendering.
 * - VolumeGridBuffer::FlowConfig runs the incompressible flow stages over a
 *   volume. Required refs left unnamed are declared under default names, and
 *   any named ref the volume does not yet hold is declared.
 * - Nodes::Network::SpatialFieldConfig drives a network's vertices through a
 *   GpuFieldOperator, reading its vertex layout from the network's own
 *   primary operator.
 *
 * All three route to the graphics domain and every call requires
 * RenderConfig::target_window. There is no default window. A null target logs
 * an error and returns nullptr, and nothing is registered. Exceptions thrown
 * by the classes' constructors propagate.
 *
 * Nothing here is realtime safe. Call from the thread that constructs the
 * objects.
 *
 * @code
 * auto life = vega.evolve(
 *     Buffers::RelaxationGridBuffer::GridConfig {
 *         .width = 256,
 *         .height = 256,
 *         .rule = { .state = Kakshya::GpuDataFormat::UINT32, .shader = "relax_conway_rule.comp" },
 *         .emit = { .state = Kakshya::GpuDataFormat::UINT32, .shader = "relax_vertex_emit.comp" },
 *     },
 *     [](uint32_t x, uint32_t y) { return (x ^ y) % 7U == 0U ? 1U : 0U; },
 *     { .target_window = window });
 *
 * auto volume = std::make_shared<Buffers::VolumeGridBuffer>(
 *     lattice,
 *     Buffers::VolumeGridBuffer::SurfaceConfig { "density", { 48, 48, 48 }, 0.5F });
 * auto density = volume->declare_scalar("density");
 * auto heat = volume->declare_scalar("heat");
 * vega.evolve(
 *     volume,
 *     { .carried = { { density, 0.99F } },
 *       .buoyancy = Buffers::VolumeGridBuffer::FlowConfig::Buoyancy { .temperature = heat, .density = density } },
 *     { .target_window = window });
 *
 * auto particles = vega.ParticleNetwork(50000, glm::vec3(-1.0F), glm::vec3(1.0F), distribution);
 * vega.evolve(
 *     particles,
 *     { .absorb_radius = 0.05F },
 *     [](const auto& field) { field->bind(Nodes::Network::FieldTarget::POSITION, swirl); },
 *     { .target_window = window });
 * @endcode
 */
class MAYAFLUX_API Evolve {
public:
    /** @brief Render target and pipeline overrides, shared by every family. */
    using RenderConfig = Buffers::VKBuffer::RenderConfig;

    /**
     * @brief Callable that binds fields on the GpuFieldOperator before it is wired.
     *
     * Bindings must exist when the geometry buffer is registered: the field pass
     * is wired only for an operator that carries at least one binding at that
     * point. The callable receives the operator with its full bind() surface,
     * cluster scoping included.
     */
    using FieldBinder = std::function<void(const std::shared_ptr<Nodes::Network::GpuFieldOperator>&)>;

    Evolve();
    ~Evolve();

    Evolve(const Evolve&) = delete;
    Evolve& operator=(const Evolve&) = delete;
    Evolve(Evolve&&) noexcept;
    Evolve& operator=(Evolve&&) noexcept;

    /**
     * @brief Build a relaxation grid, register it and attach rendering.
     * @param config Grid arrangement.
     * @param render Render target. target_window is required.
     * @return The registered grid, or nullptr if the target window is null.
     *
     * The grid is constructed from the config, registered in the graphics
     * domain, then rendering is attached. Its state is left as allocated. Use
     * the seeded overload, or RelaxationGridBuffer::seed_state, to give it an
     * initial condition.
     */
    auto operator()(
        const Buffers::RelaxationGridBuffer::GridConfig& config,
        const RenderConfig& render)
        -> std::shared_ptr<Buffers::RelaxationGridBuffer>
    {
        auto grid = build_grid(config, render, {}, 0);
        if (grid) {
            MF_LIVE_EXPOSE_AUTO(grid);
        }
        return grid;
    }

    /**
     * @brief Build a relaxation grid seeded from a host callable.
     * @param config Grid arrangement.
     * @param seed Callable taking cell coordinates (x, y) and returning the
     *        cell's state. x runs over [0, width) and y over [0, height).
     * @param render Render target. target_window is required.
     * @return The registered grid, or nullptr if the target window is null or
     *         the seed's return type does not match the grid's cell stride.
     *
     * The seed's return type is the cell type, constrained by GridCellData and
     * checked against the stride the config resolved. The state is written
     * before registration, so the first generation steps from it.
     */
    template <typename Seed>
        requires std::invocable<Seed&, uint32_t, uint32_t>
        && GridCellData<std::remove_cvref_t<std::invoke_result_t<Seed&, uint32_t, uint32_t>>>
    auto operator()(
        const Buffers::RelaxationGridBuffer::GridConfig& config,
        Seed&& seed,
        const RenderConfig& render)
        -> std::shared_ptr<Buffers::RelaxationGridBuffer>
    {
        using State = std::remove_cvref_t<std::invoke_result_t<Seed&, uint32_t, uint32_t>>;

        std::vector<std::byte> cells(static_cast<size_t>(config.width) * config.height * sizeof(State));
        for (uint32_t y = 0; y < config.height; ++y) {
            for (uint32_t x = 0; x < config.width; ++x) {
                const State value = seed(x, y);
                std::memcpy(
                    cells.data() + (static_cast<size_t>(y) * config.width + x) * sizeof(State),
                    &value,
                    sizeof(State));
            }
        }

        auto grid = build_grid(config, render, cells, sizeof(State));
        if (grid) {
            MF_LIVE_EXPOSE_AUTO(grid);
        }
        return grid;
    }

    /**
     * @brief Set the incompressible flow stages going over a volume and attach rendering.
     * @param volume Unregistered volume. Built with a SurfaceConfig if it is to be drawn.
     * @param flow Flow parameters. Required refs left with an empty name
     *        (velocity, pressure, divergence, and scratch when viscosity is
     *        above zero) are declared on the volume under default names. A
     *        named ref the volume does not yet hold is declared, with the slot
     *        count its role needs. A ref issued by another volume is left for
     *        setup_flow to reject.
     * @param render Render target. target_window is required.
     * @return The volume, or nullptr if it is null or the target window is null.
     *
     * Registers the volume in the graphics domain, which establishes its
     * chain, then builds the flow stages and attaches rendering. The stages
     * setup_flow builds are reachable afterwards through the volume's
     * processing chain by type.
     */
    auto operator()(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        const Buffers::VolumeGridBuffer::FlowConfig& flow,
        const RenderConfig& render)
        -> std::shared_ptr<Buffers::VolumeGridBuffer>
    {
        if (!build_flow(volume, flow, render)) {
            return nullptr;
        }
        MF_LIVE_EXPOSE_AUTO(volume);
        return volume;
    }

    /**
     * @brief Drive a network's vertices with fields and spatial stages, and attach rendering.
     * @tparam Network A field-compatible NodeNetwork subclass, such as ParticleNetwork.
     * @param network Unregistered network.
     * @param spatial Spatial hash, claim, density and population configuration.
     *        Default constructed means field displacement only.
     * @param bind Called with the GpuFieldOperator before the geometry buffer is
     *        registered. May be empty when the config alone is wanted.
     * @param render Render target. target_window is required.
     * @return The network, or nullptr if it is null, has no primary
     *         GraphicsOperator to read a layout from, or the target window is null.
     *
     * A ParticleNetwork with no primary operator and its default operator
     * enabled receives a PhysicsOperator. The GpuFieldOperator is created with
     * the primary operator's vertex layout and appended to the network's
     * operator chain, or reused if the chain already holds one, in which case
     * spatial is ignored. The network and its NetworkGeometryBuffer are then
     * registered in the graphics domain and rendering is attached. Registering
     * the buffer wires the field pass and every spatial stage the config
     * selects.
     */
    template <typename Network>
        requires std::is_base_of_v<Nodes::Network::NodeNetwork, Network>
    auto operator()(
        const std::shared_ptr<Network>& network,
        const Nodes::Network::SpatialFieldConfig& spatial,
        const FieldBinder& bind,
        const RenderConfig& render)
        -> std::shared_ptr<Network>
    {
        if (!build_field(std::static_pointer_cast<Nodes::Network::NodeNetwork>(network), spatial, bind, render)) {
            return nullptr;
        }
        MF_LIVE_EXPOSE_AUTO(network);
        return network;
    }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    /**
     * @brief Construct, optionally seed, register and render a relaxation grid.
     * @param config Grid arrangement.
     * @param render Render target.
     * @param seed Packed cell bytes, or empty to leave state as allocated.
     * @param seed_stride Bytes per cell in @p seed, checked against the grid's stride.
     * @return The grid, or nullptr on failure.
     */
    std::shared_ptr<Buffers::RelaxationGridBuffer> build_grid(
        const Buffers::RelaxationGridBuffer::GridConfig& config,
        const RenderConfig& render,
        const std::vector<std::byte>& seed,
        size_t seed_stride);

    /**
     * @brief Resolve refs, register, build the flow stages and render a volume.
     * @param volume Volume to run the flow over.
     * @param flow Flow parameters.
     * @param render Render target.
     * @return True when the volume was wired.
     */
    bool build_flow(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        const Buffers::VolumeGridBuffer::FlowConfig& flow,
        const RenderConfig& render);

    /**
     * @brief Create the field operator, bind, register the network and its buffer, and render.
     * @param network Network to drive.
     * @param spatial Spatial field configuration.
     * @param bind Field binder, possibly empty.
     * @param render Render target.
     * @return True when the network was wired.
     */
    bool build_field(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        const Nodes::Network::SpatialFieldConfig& spatial,
        const FieldBinder& bind,
        const RenderConfig& render);
};

} // namespace MayaFlux
