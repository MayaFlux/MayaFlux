#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes {

/**
 * @class StreamReaderNode
 * @brief Node that reads sample-by-sample from externally provided data
 *
 * Exposes set_data() to receive a block of samples from any source.
 * Each process_sample() advances a read head through that data.
 * When exhausted, returns the last valid sample (hold).
 *
 * The node does not know or care who calls set_data(). A buffer
 * processor, a pipeline dispatch, a coroutine, manual user code,
 * anything that has a span of doubles can feed it.
 *
 * Ownership: only one writer may feed data at a time. The first caller
 * to set_data() with a non-null owner claims exclusive write access.
 * Subsequent callers with a different owner are rejected until
 * release_owner() is called.
 */
class MAYAFLUX_API StreamReaderNode final : public Node {
public:
    StreamReaderNode();

    double process_sample(double input = 0.0) override;
    std::vector<double> process_batch(unsigned int num_samples) override;

    void save_state() override;
    void restore_state() override;

    NodeContext& get_last_context() override;

    /**
     * @brief Replace the internal data and reset read head to 0
     * @param data Samples to read from. Copied internally.
     * @param owner Feeder claiming write access. Rejected if another feeder owns this node.
     * @return true if data was accepted, false if a different owner holds the lock
     */
    bool set_data(std::span<const double> data, const void* owner = nullptr);

    /**
     * @brief Release ownership so another feeder can write
     * @param owner Must match the current owner
     */
    void release_owner(const void* owner);

    /**
     * @brief Number of unread samples remaining
     */
    [[nodiscard]] size_t remaining() const noexcept;

    /**
     * @brief Reset read head to 0 without changing data
     */
    void rewind() noexcept;

protected:
    void update_context(double value) override;
    void notify_tick(double value) override;

private:
    struct StreamReaderContext final : NodeContext {
        StreamReaderContext()
            : NodeContext(0.0, typeid(StreamReaderNode).name())
        {
        }
    };

    std::vector<double> m_data;
    size_t m_read_head {};
    double m_hold_value {};
    std::atomic<const void*> m_owner { nullptr };

    std::vector<double> m_saved_data;
    size_t m_saved_read_head {};
    double m_saved_hold_value {};
    double m_saved_last_output {};

    StreamReaderContext m_context;
};

} // namespace MayaFlux::Nodes
