#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"
#include "MayaFlux/Kakshya/Region/RegionSequencer.hpp"

#include "MayaFlux/Transitive/Memory/SeqLock.hpp"

namespace MayaFlux::Kakshya {

/**
 * @enum RegionDelivery
 * @brief How much of the active region one process() call delivers.
 */
enum class RegionDelivery : uint8_t {
    BLOCK, ///< A fixed number of frames of dimension 0, continuing across region boundaries
    WHOLE ///< The whole active region
};

/**
 * @struct RegionContribution
 * @brief Part of the last cycle's output that came from one region.
 *
 * For BLOCK, first and count are the frames of the block. For WHOLE, first is
 * 0 and count is the region's extent along dimension 0.
 */
struct RegionContribution {
    size_t region_index {};
    uint64_t first {};
    uint64_t count {};
};

/**
 * @class RegionWalkProcessor
 * @brief Walks the regions of one group on a container, a cycle at a time.
 *
 * Binds a named RegionGroup of the container to a RegionSequencer and owns the
 * walk across cycles. Each process() call decides which regions are ready,
 * which one is active, and what is delivered, then writes it to the container's
 * processed data and records which regions it came from.
 *
 * Readiness: a region is ready when the container has data and every
 * coordinate the region names lies inside its dimensions. The walk waits on a
 * region that is not ready and delivers silence meanwhile in BLOCK delivery,
 * and nothing in WHOLE.
 *
 * BLOCK delivery is for audio containers and attaching to any other modality
 * throws. It walks dimension 0 as frames and always produces a full block, as
 * doubles, in the container's layout: regions are joined end to end across their boundaries,
 * a GATED transition inserts silence of the group's transition_duration_ms,
 * and the block is padded with silence when the walk ends. WHOLE hands the
 * region's data over as the container produced it, for any modality. It
 * replaces the container's processed data, so on a container whose processed
 * data is also its source, such as a window's surface, it should run after
 * the capture in each cycle.
 *
 * The group's callbacks fire on the processing thread: on_region_start when a
 * region begins, on_region_end when it has been delivered, and on_transition
 * with the finished and the next region whenever the walk changes region.
 * CALLBACK transitions add nothing else. The CROSSFADE and OVERLAP transitions,
 * the ALL and OVERLAP selection patterns, GATED with WHOLE delivery, and CUSTOM
 * without a chooser are rejected when the group is bound.
 *
 * Group edits: refresh() reads the group again and publishes it through a
 * seqlock. process() adopts it at the start of the next cycle and starts the
 * walk over. Settings changed after attaching take effect at the next refresh().
 */
class MAYAFLUX_API RegionWalkProcessor : public DataProcessor {
public:
    /**
     * @param group_name Name of the container's group to walk.
     * @param delivery What one cycle delivers.
     * @param frames_per_block Block length for BLOCK delivery.
     */
    explicit RegionWalkProcessor(
        std::string group_name,
        RegionDelivery delivery = RegionDelivery::BLOCK,
        uint64_t frames_per_block = 512);

    ~RegionWalkProcessor() override = default;

    /**
     * @throws std::invalid_argument if the container is null, BLOCK delivery is asked of a
     *         container that is not audio, or the group cannot be walked.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(std::memory_order_relaxed); }

    /**
     * @brief Read the group again and walk the new one from the start.
     * @throws std::invalid_argument if the group cannot be walked.
     */
    void refresh();

    void set_seed(uint64_t seed) { m_seed = seed; }
    void set_weight_key(Facet<double> key) { m_weight_key = std::move(key); }
    void set_chooser(RegionSequencer::Chooser chooser) { m_chooser = std::move(chooser); }
    void set_frames_per_block(uint64_t frames) { m_frames_per_block = std::max<uint64_t>(frames, 1); }

    /**
     * @brief Sample rate used to turn a GATED duration into frames. Taken from a sound container on attach.
     */
    void set_sample_rate(uint32_t sample_rate) { m_sample_rate = sample_rate; }

    /**
     * @brief Called once when the walk ends.
     */
    void set_on_end(std::function<void()> callback) { m_on_end = std::move(callback); }

    /**
     * @brief The regions the last process() call drew from, in order.
     */
    [[nodiscard]] const std::vector<RegionContribution>& contributions() const noexcept { return m_contributions; }

    /**
     * @brief The region being delivered, if one is.
     */
    [[nodiscard]] std::optional<size_t> current_region() const noexcept { return m_active; }

private:
    [[nodiscard]] std::string check(const RegionGroup& group) const;
    void publish(RegionGroup group);
    void adopt_pending();

    [[nodiscard]] static bool is_ready(const Region& region, const ContainerDataStructure& structure);
    [[nodiscard]] uint64_t gate_frames(const RegionGroup& group) const;

    bool begin_region();
    void end_region();
    void finish_walk();

    void deliver_block(SignalSourceContainer& container);
    void deliver_whole(SignalSourceContainer& container);

    std::string m_group_name;
    RegionDelivery m_delivery;
    uint64_t m_frames_per_block;
    uint64_t m_seed {};
    uint32_t m_sample_rate {};

    std::optional<Facet<double>> m_weight_key;
    RegionSequencer::Chooser m_chooser;
    std::function<void()> m_on_end;

    std::weak_ptr<SignalSourceContainer> m_container_weak;
    std::atomic<bool> m_is_processing { false };

    Memory::Seqlock m_pending_lock;
    RegionGroup m_pending_group;
    uint32_t m_adopted_sequence {};

    std::unique_ptr<RegionSequencer> m_sequencer;
    std::optional<size_t> m_active;
    std::optional<size_t> m_previous;
    uint64_t m_position {};
    uint64_t m_gate_remaining {};
    bool m_ended {};

    std::vector<RegionContribution> m_contributions;
    std::vector<std::vector<double>> m_block;
};

}
