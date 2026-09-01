#pragma once

#include "MayaFlux/Portal/Forma/Bridge.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Portal/Forma/Plot/Plot.hpp"

namespace MayaFlux::Nodes {
class NodeGraphManager;
}

namespace MayaFlux::Core {
class Window;
class WindowManager;
struct WindowCreateInfo;
}

namespace MayaFlux::Vruta {
class TaskScheduler;
class EventManager;
}

namespace MayaFlux::Buffers {
class BufferManager;
}

namespace MayaFlux::Portal::Forma {

class Inspector;

namespace internal {

    constexpr size_t k_capacity_bytes = 4096;

    /**
     * @class Atelier
     * @brief Process-wide Forma workshop. Not part of the public surface.
     *
     * Holds the engine references stored by Portal::Forma::initialize and
     * owns every construction path that depends on them: buffers, layers,
     * contexts, surfaces, elements, windows.
     *
     * Everything under Internal/ is implementation detail and carries no
     * stability guarantee. The public entry points in Forma.hpp are thin
     * wrappers over the methods here. User code calls those, not these.
     *
     * Lifetime follows the Portal::Graphics singleton contract: the Meyers
     * instance is never destroyed, and shutdown() releases the held
     * references explicitly before static teardown.
     */
    class MAYAFLUX_API Atelier {
    public:
        static Atelier& instance()
        {
            static Atelier a;
            return a;
        }

        Atelier(const Atelier&) = delete;
        Atelier& operator=(const Atelier&) = delete;
        Atelier(Atelier&&) = delete;
        Atelier& operator=(Atelier&&) = delete;

        // =====================================================================
        // Lifecycle
        // =====================================================================

        bool initialize(
            std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager,
            std::shared_ptr<Buffers::BufferManager> buffer_manager,
            std::shared_ptr<Vruta::TaskScheduler> scheduler,
            std::shared_ptr<Vruta::EventManager> event_manager,
            std::shared_ptr<Core::WindowManager> window_manager);

        void shutdown();

        [[nodiscard]] bool is_initialized() const noexcept { return m_initialized; }

        /**
         * @brief Bridge instance constructed during initialize.
         * @throws std::runtime_error if called before initialize().
         */
        [[nodiscard]] Bridge& bridge();

        /**
         * @brief Inspector instance constructed during initialize.
         * @throws std::runtime_error if called before initialize().
         */
        [[nodiscard]] Inspector& inspector();

        // =====================================================================
        // Construction
        // =====================================================================

        /**
         * @brief Create a window through the stored WindowManager and show it.
         *
         * Present so that callers needing a window do not reach the manager
         * directly.
         */
        [[nodiscard]] std::shared_ptr<Core::Window> create_window(
            const Core::WindowCreateInfo& info);

        /**
         * @brief Capacity-explicit FormaBuffer construction.
         *
         * The single buffer construction path. Registers with the stored
         * BufferManager under GRAPHICS_BACKEND and calls setup_rendering
         * with whichever texture configuration was requested.
         */
        [[nodiscard]] std::shared_ptr<Buffers::FormaBuffer> create_buffer(
            std::shared_ptr<Core::Window> window,
            size_t capacity,
            Graphics::PrimitiveTopology topology,
            const std::string& texture_binding = {},
            std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>>
                additional_textures = {});

        /**
         * @brief Construct a Layer and a Context wired to @p window.
         *
         * The Context is stored so its event coroutines outlive the call.
         */
        [[nodiscard]] std::pair<std::shared_ptr<Layer>, std::shared_ptr<Context>>
        create_layer(const std::shared_ptr<Core::Window>& window, std::string name);

        /// @brief create_layer plus ownership of the window, as a Surface.
        [[nodiscard]] Surface create_surface(
            std::shared_ptr<Core::Window> window, std::string name);

        /**
         * @brief Build a FormaBuffer, construct a Mapped<T>, register the
         *        element on @p layer, and register it with the Bridge.
         *
         * @tparam T       MappedState value type.
         * @param layer    Layer to register the element on.
         * @param window   Target window for rendering.
         * @param geom     Geometry function producing vertex bytes from T.
         * @param initial  Starting value written into MappedState.
         * @param topology Primitive topology for the FormaBuffer.
         * @param capacity Initial FormaBuffer capacity in bytes.
         * @param project  Optional T to float projection for outbound readers.
         */
        template <typename T>
        [[nodiscard]] Mapped<T> create_element(
            Layer& layer,
            std::shared_ptr<Core::Window> window,
            GeometryFn<T> geom,
            T initial,
            Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP,
            size_t capacity = k_capacity_bytes,
            std::function<float(T)> project = {})
        {
            auto buf = create_buffer(std::move(window), capacity, topology);
            auto mapped = make_mapped<T>(std::move(initial), std::move(geom), std::move(buf));
            mapped.element.id = layer.add(mapped.element);
            bridge().register_element(mapped, std::move(project));
            return mapped;
        }

        /**
         * @brief create_element against a Surface, followed by one sync so
         *        bounds_hint and contains from the geometry function reach
         *        the Layer before the first frame.
         */
        template <typename T>
        [[nodiscard]] Mapped<T> create_element(
            Surface& surface,
            GeometryFn<T> geom,
            T initial,
            Graphics::PrimitiveTopology topology = Graphics::PrimitiveTopology::TRIANGLE_STRIP,
            std::function<float(T)> project = {})
        {
            auto mapped = create_element<T>(
                surface.layer(), surface.window(),
                std::move(geom), std::move(initial),
                topology, k_capacity_bytes, std::move(project));

            mapped.sync();
            if (mapped.element.bounds_hint)
                surface.layer().set_bounds(mapped.element.id, *mapped.element.bounds_hint);
            if (mapped.element.contains)
                surface.layer().set_contains(mapped.element.id, mapped.element.contains);

            return mapped;
        }

        /**
         * @brief Create and place every adornment declared on @p spec.
         *
         * Builds one buffer per axis label, tick label, legend swatch, and
         * legend label, then hands each to the corresponding Plot placement
         * function. Each adornment is related to @p relate_to so removal and
         * visibility cascade from the series element.
         *
         * Lives here rather than in Plot because it constructs buffers,
         * which requires the BufferManager. Plot composes placement from
         * arguments it is given.
         *
         * @param surface   Surface to place on.
         * @param spec      Series spec carrying the adornment declarations.
         * @param relate_to Element id the adornments are related to.
         */
        void place_adornments(
            Surface& surface,
            const Plot::SeriesSpec& spec,
            uint32_t relate_to);

    private:
        Atelier();
        ~Atelier();

        std::shared_ptr<Nodes::NodeGraphManager> m_node_graph_manager;
        std::shared_ptr<Buffers::BufferManager> m_buffer_manager;
        std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
        std::shared_ptr<Vruta::EventManager> m_event_manager;
        std::shared_ptr<Core::WindowManager> m_window_manager;

        std::unique_ptr<Bridge> m_bridge;
        std::unique_ptr<Inspector> m_inspect;

        bool m_initialized {};
    };

    /// @brief Accessor for the process-wide Atelier.
    inline Atelier& atelier() { return Atelier::instance(); }

} // namespace internal
} // namespace MayaFlux::Portal::Forma
