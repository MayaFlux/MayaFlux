#include "Evolve.hpp"

#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Buffers/Network/NetworkGeometryBuffer.hpp"

#include "MayaFlux/Nodes/Network/Operators/GpuFieldOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"
#include "MayaFlux/Nodes/Network/Operators/PhysicsOperator.hpp"
#include "MayaFlux/Nodes/Network/ParticleNetwork.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

namespace {

    /**
     * @brief Whether the volume already holds a field of this name.
     * @param volume Volume to query.
     * @param name Field name.
     * @return True if declared.
     */
    bool has_field(const std::shared_ptr<Buffers::VolumeGridBuffer>& volume, const std::string& name)
    {
        const auto names = volume->get_field_names();
        return std::ranges::find(names, name) != names.end();
    }

    /**
     * @brief Whether a ref was issued by a volume other than the one being wired.
     * @param owner Owner recorded on the ref.
     * @param volume Volume being wired.
     * @return True if the ref names a different, live volume.
     */
    bool owned_elsewhere(
        const std::weak_ptr<Buffers::VolumeGridBuffer>& owner,
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume)
    {
        const auto held = owner.lock();
        return held && held != volume;
    }

    /**
     * @brief Resolve a scalar ref against a volume, declaring the field if absent.
     * @param volume Volume being wired.
     * @param ref Ref supplied by the caller.
     * @param fallback Name given to an unnamed ref. Empty leaves it unnamed.
     * @param single_slot True to declare a single-slot field, false for a
     *        double-buffered one.
     * @return A ref issued by the volume, or the input unchanged when it cannot
     *         be resolved.
     */
    Buffers::ScalarRef resolve_scalar(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        Buffers::ScalarRef ref,
        std::string_view fallback,
        bool single_slot)
    {
        if (ref.name.empty()) {
            if (fallback.empty()) {
                return ref;
            }
            ref.name = std::string(fallback);
        }

        if (owned_elsewhere(ref.owner, volume)) {
            return ref;
        }

        if (!has_field(volume, ref.name)) {
            return single_slot ? volume->declare_scratch(ref.name) : volume->declare_scalar(ref.name);
        }

        ref.owner = volume;
        return ref;
    }

    /**
     * @brief Resolve a vector ref against a volume, declaring the field if absent.
     * @param volume Volume being wired.
     * @param ref Ref supplied by the caller.
     * @param fallback Name given to an unnamed ref. Empty leaves it unnamed.
     * @return A ref issued by the volume, or the input unchanged when it cannot
     *         be resolved.
     */
    Buffers::VectorRef resolve_vector(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        Buffers::VectorRef ref,
        std::string_view fallback)
    {
        if (ref.name.empty()) {
            if (fallback.empty()) {
                return ref;
            }
            ref.name = std::string(fallback);
        }

        if (owned_elsewhere(ref.owner, volume)) {
            return ref;
        }

        if (!has_field(volume, ref.name)) {
            return volume->declare_vector(ref.name);
        }

        ref.owner = volume;
        return ref;
    }

    /**
     * @brief Whether a render config names a window to draw into.
     * @param render Render config.
     * @return True when target_window is set. Logs an error otherwise.
     */
    bool has_target(const Evolve::RenderConfig& render)
    {
        if (render.target_window) {
            return true;
        }

        MF_ERROR(Journal::Component::API, Journal::Context::Init,
            "evolve: RenderConfig::target_window is required, there is no default window");
        return false;
    }

} // namespace

/**
 * @struct Evolve::Impl
 * @brief Wiring sequences behind Evolve. Holds no state.
 */
struct Evolve::Impl {
    /**
     * @brief Fill unresolved refs of a flow config from the volume.
     * @param volume Volume being wired.
     * @param config Flow config, edited in place.
     *
     * Velocity, pressure and divergence are always required, so an unnamed one
     * is declared as "velocity", "pressure" and "divergence". Scratch is
     * required only when viscosity is above zero. Carried and buoyancy refs
     * carry the caller's own names and are declared only if the volume does
     * not hold them.
     */
    void resolve(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        Buffers::VolumeGridBuffer::FlowConfig& config) const
    {
        config.velocity = resolve_vector(volume, config.velocity, "velocity");
        config.pressure = resolve_scalar(volume, config.pressure, "pressure", false);
        config.divergence = resolve_scalar(volume, config.divergence, "divergence", true);

        if (config.viscosity > 0.0F) {
            config.scratch = resolve_vector(volume, config.scratch, "scratch");
        }

        for (auto& carried : config.carried) {
            carried.field = resolve_scalar(volume, carried.field, {}, false);
        }

        if (config.buoyancy) {
            config.buoyancy->temperature = resolve_scalar(volume, config.buoyancy->temperature, {}, false);
            config.buoyancy->density = resolve_scalar(volume, config.buoyancy->density, {}, false);
        }
    }

    [[nodiscard]] std::shared_ptr<Buffers::RelaxationGridBuffer> build_grid(
        const Buffers::RelaxationGridBuffer::GridConfig& config,
        const RenderConfig& render,
        const std::vector<std::byte>& seed,
        size_t seed_stride) const
    {
        if (!has_target(render)) {
            return nullptr;
        }

        auto grid = std::make_shared<Buffers::RelaxationGridBuffer>(config);

        if (!seed.empty()) {
            if (seed_stride != grid->get_cell_stride_bytes()) {
                MF_ERROR(Journal::Component::API, Journal::Context::Init,
                    "evolve: seed cell is {} bytes but the grid's cell format is {} bytes",
                    seed_stride, grid->get_cell_stride_bytes());
                return nullptr;
            }
            grid->seed_state(seed.data(), seed.size());
        }

        register_graphics_buffer(grid);
        grid->setup_rendering(render);
        return grid;
    }

    [[nodiscard]] bool build_flow(
        const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
        const Buffers::VolumeGridBuffer::FlowConfig& flow,
        const RenderConfig& render) const
    {
        if (!volume) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "evolve: null VolumeGridBuffer");
            return false;
        }

        if (!has_target(render)) {
            return false;
        }

        Buffers::VolumeGridBuffer::FlowConfig config = flow;
        resolve(volume, config);

        register_graphics_buffer(volume);
        (void)volume->setup_flow(config);
        volume->setup_rendering(render);
        return true;
    }

    [[nodiscard]] bool build_field(
        const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        const Nodes::Network::SpatialFieldConfig& spatial,
        const FieldBinder& bind,
        const RenderConfig& render) const
    {
        if (!network) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "evolve: null NodeNetwork");
            return false;
        }

        if (!has_target(render)) {
            return false;
        }

        if (!network->has_operator()) {
            auto particles = std::dynamic_pointer_cast<Nodes::Network::ParticleNetwork>(network);
            if (particles && particles->default_operator_enabled()) {
                particles->create_operator<Nodes::Network::PhysicsOperator>();
            }
        }

        const auto* primary = dynamic_cast<const Nodes::Network::GraphicsOperator*>(network->get_operator());
        if (!primary) {
            MF_ERROR(Journal::Component::API, Journal::Context::Init,
                "evolve: network has no primary GraphicsOperator to read a vertex layout from");
            return false;
        }

        auto chain = network->get_operator_chain();
        auto field = chain->find<Nodes::Network::GpuFieldOperator>();
        if (!field) {
            field = chain->emplace<Nodes::Network::GpuFieldOperator>(primary->get_vertex_layout(), spatial);
        } else {
            MF_WARN(Journal::Component::API, Journal::Context::Init,
                "evolve: network already carries a GpuFieldOperator, its SpatialFieldConfig is kept");
        }

        if (bind) {
            bind(field);
        }

        register_node_network(network, Nodes::ProcessingToken::VISUAL_RATE);

        auto buffer = std::make_shared<Buffers::NetworkGeometryBuffer>(network);
        register_graphics_buffer(buffer);
        buffer->setup_rendering(render);
        return true;
    }
};

Evolve::Evolve()
    : m_impl(std::make_unique<Impl>())
{
}

Evolve::~Evolve() = default;

Evolve::Evolve(Evolve&&) noexcept = default;

Evolve& Evolve::operator=(Evolve&&) noexcept = default;

std::shared_ptr<Buffers::RelaxationGridBuffer> Evolve::build_grid(
    const Buffers::RelaxationGridBuffer::GridConfig& config,
    const RenderConfig& render,
    const std::vector<std::byte>& seed,
    size_t seed_stride)
{
    return m_impl->build_grid(config, render, seed, seed_stride);
}

bool Evolve::build_flow(
    const std::shared_ptr<Buffers::VolumeGridBuffer>& volume,
    const Buffers::VolumeGridBuffer::FlowConfig& flow,
    const RenderConfig& render)
{
    return m_impl->build_flow(volume, flow, render);
}

bool Evolve::build_field(
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    const Nodes::Network::SpatialFieldConfig& spatial,
    const FieldBinder& bind,
    const RenderConfig& render)
{
    return m_impl->build_field(network, spatial, bind, render);
}

} // namespace MayaFlux
