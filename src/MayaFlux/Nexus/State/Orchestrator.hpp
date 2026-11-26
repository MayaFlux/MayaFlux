#pragma once

#include "MayaFlux/Nexus/Principals/Locus.hpp"
#include "MayaFlux/Nexus/State/Bindings.hpp"
#include "MayaFlux/Nexus/Tapestry.hpp"

#include "MayaFlux/Kriya/Realization/MeshAssemblyRealization.hpp"
#include "MayaFlux/Transitive/Reflect/Describe.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Nexus {

/**
 * @class StateOrchestrator
 * @brief Makes Nexus principals, one at a time or for a whole assembly.
 *
 * Each piece returns the principal it names. Given a Tapestry, every principal
 * made here is also wired into one Fabric created in it; without one they are
 * returned unwired. orchestrate() is these pieces used on every row.
 *
 * Buffers are made but never registered or drawn, and nothing moves until
 * motion_from() is asked.
 *
 * @code
 * Nexus::StateOrchestrator orchestrator(scheduler, tapestry);
 *
 * auto eye = orchestrator.locus_from(assembly, 3);
 * auto lamp = orchestrator.emitter_from(assembly, 2);
 * auto body = orchestrator.agent_from(mesh_buffer);
 *
 * auto [realization, rows] = orchestrator.orchestrate(assembly);
 * auto [placing, deforming, streaming] = orchestrator.motion_from(realization);
 * @endcode
 */
class MAYAFLUX_API StateOrchestrator {
public:
    /** @brief Decides what a row of an assembly becomes. A null pointer means nothing. */
    using RowRule = std::function<Fabric::Member(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row)>;

    /** @brief Row and principal pairs in row order. */
    using Rows = std::vector<std::pair<uint64_t, Fabric::Member>>;

    /**
     * @brief Decides how a principal is wired, before the wiring is finalised.
     *
     * @p row is the assembly row the principal was made from, or nullopt for one
     * made from plain values or a buffer.
     */
    using WiringRule = std::function<void(Wiring&, std::optional<uint64_t> row)>;

    /** @brief A buffer, and the slot of it when it holds several things. */
    using BufferSlot = std::pair<std::shared_ptr<Buffers::VKBuffer>, std::optional<uint32_t>>;

    /**
     * @brief The reflows that move one realization: placements, mesh frames and point
     *        or curve frames. Dropping the last reference to a reflow stops it.
     */
    using Reflows = std::tuple<
        std::shared_ptr<Kriya::MotionReflow>,
        std::vector<std::shared_ptr<Kriya::MeshReflow>>,
        std::vector<std::shared_ptr<Kriya::SpatialReflow>>>;

    /**
     * @param scheduler Where reflows run.
     * @param tapestry  Where principals are wired. Null returns them unwired. A Fabric
     *                  is created in it when the first principal needs one.
     */
    explicit StateOrchestrator(
        const std::shared_ptr<Vruta::TaskScheduler>& scheduler,
        const std::shared_ptr<Tapestry>& tapestry = nullptr)
        : m_scheduler(scheduler)
        , m_tapestry(tapestry)
    {
    }

    /** @brief The Fabric principals are wired into, or null before the first one. */
    [[nodiscard]] const std::shared_ptr<Fabric>& fabric() const { return m_fabric; }

    /**
     * @brief How each principal is wired. Without one it is commit driven.
     *
     * A place_from set here replaces the one a buffer's principal is given.
     */
    void set_wiring_rule(WiringRule rule) { m_wiring_rule = std::move(rule); }

    /** @brief Replace what a row becomes in orchestrate(). Empty returns to default_row(). */
    void set_row_rule(RowRule rule) { m_row_rule = std::move(rule); }

    /**
     * @brief The image of each material orchestrate() uses.
     *
     * Without one, a path given to orchestrate() finds textures beside that file,
     * and an assembly given directly gets none. A material is bound before the
     * realization is returned, so present it afterwards. The part that binds an
     * image retains it; release the hold your resolver took.
     */
    void set_material_resolver(Kriya::MeshAssemblyMaterialResolver resolver) { m_material_resolver = std::move(resolver); }

    /**
     * @brief Name what is made here, so a saved state can find it again.
     *
     * Every followed buffer is added to the bindings under a name, with the file
     * and row it came from when orchestrate() read one, and its following is
     * wired through a follow_buffer() call. follow_buffer is exposed in the
     * bindings' functions if it is not already.
     */
    void set_bindings(const std::shared_ptr<State::Bindings>& bindings);

    /** @brief How orchestrate() cuts an assembly into buffers. */
    void set_realize_options(Kriya::MeshAssemblyRealizeOptions options) { m_realize_options = std::move(options); }

    /**
     * @brief The rows whose principal is a T, in row order.
     *
     * Asking for Agent includes its children, and asking for a child such as Locus
     * picks those out.
     */
    template <typename T>
    [[nodiscard]] static std::vector<std::pair<uint64_t, std::shared_ptr<T>>> rows_as(const Rows& rows)
    {
        std::vector<std::pair<uint64_t, std::shared_ptr<T>>> found;
        for (const auto& [row, member] : rows) {
            std::visit([&found, row](const auto& ptr) {
                using Held = typename std::decay_t<decltype(ptr)>::element_type;
                if constexpr (std::is_base_of_v<T, Held>) {
                    found.emplace_back(row, ptr);
                } else if constexpr (std::is_base_of_v<Held, T> && std::is_polymorphic_v<Held>) {
                    if (auto child = std::dynamic_pointer_cast<T>(ptr)) {
                        found.emplace_back(row, std::move(child));
                    }
                }
            },
                member);
        }
        return found;
    }

    /**
     * @brief The row whose principal has an id, such as one a spatial query answered.
     * @return nullopt when no principal of @p rows has it, and for id 0.
     */
    [[nodiscard]] static std::optional<uint64_t> row_with_id(const Rows& rows, uint32_t id)
    {
        if (id != 0) {
            for (const auto& [row, member] : rows) {
                if (std::visit([](const auto& ptr) { return ptr->id(); }, member) == id) {
                    return row;
                }
            }
        }
        return std::nullopt;
    }

    // =========================================================================
    // Pieces
    // =========================================================================

    /** @brief A Locus looking from @p view. */
    [[nodiscard]] std::shared_ptr<Locus> locus_from(const Kinesis::Viewpoint& view);

    /**
     * @brief A Locus looking out from a row, whatever the row shows.
     *
     * It takes the lens of a camera row.
     * @return nullptr for a null assembly or a row out of range.
     */
    [[nodiscard]] std::shared_ptr<Locus> locus_from(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row);

    /** @brief An Emitter with the color of @p light, placed at @p placement. */
    [[nodiscard]] std::shared_ptr<Emitter> emitter_from(
        const Kakshya::MeshAssemblyLight& light, const glm::mat4& placement);

    /**
     * @brief An Emitter where a row rests, whatever the row shows.
     *
     * It takes the color of a light row.
     * @return nullptr for a null assembly or a row out of range.
     */
    [[nodiscard]] std::shared_ptr<Emitter> emitter_from(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row);

    /**
     * @brief An Agent, or a child of it, for a buffer or one slot of it.
     *
     * It is placed where the buffer is drawn now. Wired here it also follows the
     * buffer, through placement_from(). It only reads the buffer; use attach()
     * on the result to move the buffer instead.
     *
     * @tparam T Agent or a class derived from it.
     * @param buffer Buffer to stand for.
     * @param slot   Slot of a mesh or instance network, if not all of it.
     * @param args   Constructor arguments of T.
     * @return nullptr for a null buffer.
     */
    template <typename T = Agent, typename... Args>
    [[nodiscard]] std::shared_ptr<T> agent_from(
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        std::optional<uint32_t> slot = std::nullopt,
        Args&&... args)
    {
        static_assert(std::is_base_of_v<Agent, T>, "agent_from makes an Agent or a child of it");
        if (!buffer) {
            return nullptr;
        }
        auto agent = std::make_shared<T>(std::forward<Args>(args)...);
        wrap(Fabric::Member { std::shared_ptr<Agent>(agent) }, { buffer, slot });
        return agent;
    }

    /**
     * @brief A source that keeps something where a buffer or one slot of it is drawn.
     *
     * For Wiring::place_from. The position is the buffer's anchor, as track_anchor()
     * answers it, so a skinned slot is followed through its palette, carried by the
     * transform the buffer is drawn with; the orientation is that transform's
     * rotation. Until an anchor can be read, nothing is set. Points and curves are
     * placed by their reflow's placement sink. Reading a buffer a compute pass drives
     * blocks. Wrap the result to add an offset of your own. The buffer is not
     * retained.
     */
    [[nodiscard]] static Wiring::PlacementFn placement_from(
        const std::shared_ptr<Buffers::VKBuffer>& buffer,
        std::optional<uint32_t> slot = std::nullopt);

    /**
     * @brief The buffer and slot that show a row of a realization.
     * @return A null buffer for a row nothing draws.
     */
    [[nodiscard]] static BufferSlot buffer_of(
        const std::shared_ptr<const Kriya::MeshAssemblyRealization>& realization, uint64_t row);

    /**
     * @brief Start moving a realization.
     *
     * Plays one motion of its assembly through the placement sink, and every source
     * that has frames of its own through a reflow of its own. Present the
     * realization first. Nothing is held here: keep what is returned.
     *
     * @param realization What moves. Null gives nothing.
     * @param motion      Motion of the assembly. Out of range plays only the frames.
     * @param edge        What every course does at its ends.
     * @param paused      Whether the courses start paused.
     * @return The reflows, empty without a scheduler.
     */
    [[nodiscard]] Reflows motion_from(
        const std::shared_ptr<const Kriya::MeshAssemblyRealization>& realization,
        size_t motion = 0,
        Kriya::CourseEdge edge = Kriya::CourseEdge::Loop,
        bool paused = false) const;

    // =========================================================================
    // Whole assembly
    // =========================================================================

    /**
     * @brief What a row becomes without a rule, unwired and unplaced.
     *
     * A camera is a Locus with its lens, a light an Emitter with its color, a
     * mesh, points or curves an Agent, and a joint nothing.
     */
    [[nodiscard]] static Fabric::Member default_row(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly, size_t row);

    /**
     * @brief Realize an assembly and make a principal for each row that becomes one.
     *
     * A principal of a drawn row stands for its buffer, as agent_from() does, and
     * any other stands where its row rests. A position or orientation the row rule
     * already set is kept until a followed buffer says otherwise. A Locus takes the
     * row's viewpoint. A principal the rule already wired is left as it is.
     *
     * Present the realization yourself, and add a Locus's view targets once the
     * render processors exist.
     *
     * @param assembly Source of rows. Null gives nothing.
     * @return The realization, null when nothing is drawn, and the principals.
     */
    [[nodiscard]] std::pair<std::shared_ptr<Kriya::MeshAssemblyRealization>, Rows> orchestrate(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly);

    /**
     * @brief Read a 3D model or Alembic file and orchestrate it.
     * @param filepath Path to the file.
     * @return The assembly, its realization and the principals, all empty on failure.
     */
    [[nodiscard]] std::tuple<
        std::shared_ptr<Kakshya::MeshAssemblyContainer>,
        std::shared_ptr<Kriya::MeshAssemblyRealization>,
        Rows>
    orchestrate(const std::string& filepath);

private:
    /** Realize, bind materials with @p materials and make the rows. */
    [[nodiscard]] std::pair<std::shared_ptr<Kriya::MeshAssemblyRealization>, Rows> orchestrate_rows(
        const std::shared_ptr<const Kakshya::MeshAssemblyContainer>& assembly,
        const Kriya::MeshAssemblyMaterialResolver& materials,
        const std::string& file = {});

    /** Place a principal where its buffer is drawn, then wire it to follow. */
    void wrap(const Fabric::Member& member, const BufferSlot& shown,
        const std::string& file = {}, std::optional<uint64_t> row = std::nullopt);

    /** Wire a principal into the Fabric, when there is a Tapestry and it is not wired yet. */
    void admit(const Fabric::Member& member, Wiring::PlacementFn follow = {},
        const Reflect::FunctionCall& call = {}, std::optional<uint64_t> row = std::nullopt);

    /** The bound name of a buffer, adding it and its source when it has none. */
    [[nodiscard]] std::string name_buffer(const std::shared_ptr<Buffers::VKBuffer>& buffer,
        const std::string& file, std::optional<uint64_t> row);

    std::shared_ptr<Vruta::TaskScheduler> m_scheduler;
    std::shared_ptr<Tapestry> m_tapestry;
    std::shared_ptr<Fabric> m_fabric;
    WiringRule m_wiring_rule;
    RowRule m_row_rule;
    Kriya::MeshAssemblyRealizeOptions m_realize_options;
    Kriya::MeshAssemblyMaterialResolver m_material_resolver;
    std::shared_ptr<State::Bindings> m_bindings;
};

/**
 * @brief StateOrchestrator::placement_from as a free function, so a saved call can make it.
 *
 * Expose it in a Reflect::FunctionTable whose table holds the buffers by name.
 *
 * @code
 * using Followed = std::map<std::string, std::shared_ptr<Buffers::VKBuffer>, std::less<>>;
 *
 * Reflect::FunctionTable<Followed> calls;
 * calls.expose<&Nexus::follow_buffer>();
 *
 * Reflect::FunctionCall call { "follow_buffer", { { "buffer", "wolf" }, { "slot", "3" } } };
 * std::string error;
 * if (auto follow = calls.make<Nexus::Wiring::PlacementFn>(call, error, followed)) {
 *     fabric->wire(agent).place_from(call, *follow).finalise();
 * }
 * @endcode
 */
[[nodiscard]] MAYAFLUX_API Wiring::PlacementFn follow_buffer(
    const std::shared_ptr<Buffers::VKBuffer>& buffer,
    std::optional<uint32_t> slot = std::nullopt);

} // namespace MayaFlux::Nexus

MF_DESCRIBE_FUNCTION(&MayaFlux::Nexus::follow_buffer)

#include "Orchestrator.signatures.hpp"
