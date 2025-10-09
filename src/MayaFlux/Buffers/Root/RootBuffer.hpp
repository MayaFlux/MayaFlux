#pragma once

#include "MayaFlux/Buffers/BufferUtils.hpp"

namespace MayaFlux::Buffers {

template <typename BufferType>
class RootBuffer : public BufferType {
public:
    using BufferType::BufferType;

    /**
     * @brief Adds a tributary buffer to this root buffer
     * @param buffer Child buffer to add to the aggregation hierarchy
     *
     * Tributary buffers contribute their data to the root buffer
     * when the root buffer is processed. This allows multiple computational
     * streams to be combined into a single output channel.
     *
     * @throws std::runtime_error if buffer is not acceptable based on current token enforcement strategy
     */
    virtual void add_child_buffer(std::shared_ptr<BufferType> buffer)
    {
        if (this->is_processing()) {
            for (size_t i = 0; i < MAX_PENDING; ++i) {
                bool expected = false;
                if (m_pending_ops[i].active.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
                    m_pending_ops[i].buffer = buffer;
                    m_pending_ops[i].is_addition = true;
                    m_pending_count.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }

            return;
        }
        add_child_buffer_direct(buffer);
    }

    /**
     * @brief Attempts to add a child buffer without throwing exceptions
     * @param buffer Child buffer to add to the aggregation hierarchy
     * @param rejection_reason Optional output parameter for rejection reason
     * @return True if buffer was successfully added, false otherwise
     *
     * This is a non-throwing version of add_child_buffer() that can be used
     * when you want to handle rejection gracefully without exception handling.
     */
    virtual bool try_add_child_buffer(std::shared_ptr<BufferType> buffer, std::string* rejection_reason = nullptr)
    {
        if (!is_buffer_acceptable(buffer, rejection_reason)) {
            return false;
        }

        add_child_buffer(buffer);
        return true;
    }

    /**
     * @brief Removes a tributary buffer from this root buffer
     * @param buffer Child buffer to remove from the aggregation hierarchy
     */
    virtual void remove_child_buffer(std::shared_ptr<BufferType> buffer)
    {
        if (this->is_processing()) {
            for (size_t i = 0; i < MAX_PENDING; ++i) {
                bool expected = false;
                if (m_pending_ops[i].active.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
                    m_pending_ops[i].buffer = buffer;
                    m_pending_ops[i].is_addition = false;
                    m_pending_count.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }

            return;
        }

        remove_child_buffer_direct(buffer);
    }

    /**
     * @brief Gets the number of tributary buffers in the aggregation hierarchy
     * @return Number of tributary buffers
     */
    size_t get_num_children() const { return m_child_buffers.size(); }

    /**
     * @brief Gets all tributary buffers in the aggregation hierarchy
     * @return Constant reference to the vector of tributary buffers
     */
    const std::vector<std::shared_ptr<BufferType>>& get_child_buffers() const
    {
        return m_child_buffers;
    }

    /**
     * @brief Resets all data values in this buffer and its tributaries
     *
     * Initializes all sample values to zero in this buffer and optionally
     * in all tributary buffers in the aggregation hierarchy.
     */
    virtual void clear() override
    {
        BufferType::clear();
        for (auto& child : m_child_buffers) {
            child->clear();
        }
    }

    /**
     * @brief Activates/deactivates processing for the current token
     * @param active Whether this buffer should process when its token is active
     *
     * This allows subsystems to selectively enable/disable buffers based on
     * current processing requirements without changing the token assignment.
     */
    virtual void set_token_active(bool active) = 0;

    /**
     * @brief Checks if the buffer is active for its assigned token
     * @return True if the buffer will process when its token is processed
     */
    virtual bool is_token_active() const = 0;

    /**
     * @brief Sets processing rate hint for the buffer
     * @param samples_per_second Expected processing rate for this token
     *
     * This helps the buffer optimize its processing for different rates.
     * Audio might be 48kHz, visual might be 60Hz, custom might be variable.
     */
    virtual void set_processing_rate_hint(uint32_t tick_rate) { m_processing_rate_hint = tick_rate; }

    /**
     * @brief Gets the processing rate hint
     * @return Expected processing rate in samples/frames per second
     */
    virtual uint32_t get_processing_rate_hint() const { return m_processing_rate_hint; }

    /**
     * @brief Enables cross-modal data sharing
     * @param enabled Whether this buffer should share data across processing domains
     *
     * When enabled, the buffer can be accessed by different subsystems
     * simultaneously, enabling advanced cross-modal processing.
     */
    virtual void enable_cross_modal_sharing(bool enabled) { m_cross_modal_sharing = enabled; }

    /**
     * @brief Checks if cross-modal sharing is enabled
     * @return True if buffer can be shared across processing domains
     */
    virtual bool is_cross_modal_sharing_enabled() const { return m_cross_modal_sharing; }

    /**
     * @brief Validates if a buffer is acceptable based on current token enforcement strategy
     * @param buffer Buffer to validate
     * @param rejection_reason Optional output parameter for rejection reason
     * @return True if buffer is acceptable, false otherwise
     *
     * This method encapsulates all token compatibility validation logic based on the
     * current enforcement strategy. It provides a clean separation between validation
     * logic and the actual buffer addition process, making the code more maintainable
     * and testable.
     */
    bool is_buffer_acceptable(std::shared_ptr<BufferType> buffer, std::string* rejection_reason = nullptr) const
    {
        auto default_processor = buffer->get_default_processor();
        if (!default_processor) {
            return true;
        }

        ProcessingToken child_token = default_processor->get_processing_token();

        switch (m_token_enforcement_strategy) {
        case TokenEnforcementStrategy::STRICT:
            if (child_token != m_preferred_processing_token) {
                if (rejection_reason) {
                    *rejection_reason = "Child buffer's default processor token does not match preferred processing token (STRICT mode)";
                }
                return false;
            }
            break;

        case TokenEnforcementStrategy::FILTERED:
            if (!are_tokens_compatible(m_preferred_processing_token, child_token)) {
                if (rejection_reason) {
                    *rejection_reason = "Child buffer's default processor token is not compatible with preferred processing token (FILTERED mode)";
                }
                return false;
            }
            break;

        case TokenEnforcementStrategy::OVERRIDE_SKIP:
            if (!are_tokens_compatible(m_preferred_processing_token, child_token)) {
                if (rejection_reason) {
                    *rejection_reason = "Child buffer token is incompatible but will be conditionally processed (OVERRIDE_SKIP mode)";
                }
            }
            break;

        case TokenEnforcementStrategy::OVERRIDE_REJECT:
            if (!are_tokens_compatible(m_preferred_processing_token, child_token)) {
                if (rejection_reason) {
                    *rejection_reason = "Child buffer token is incompatible and will be removed later (OVERRIDE_REJECT mode)";
                }
            }
            break;

        case TokenEnforcementStrategy::IGNORE:
            break;
        }

        return true;
    }

    bool has_pending_operations() const
    {
        return m_pending_count.load(std::memory_order_relaxed) > 0;
    }

protected:
    void add_child_buffer_direct(std::shared_ptr<BufferType> buffer)
    {
        std::string rejection_reason;
        if (!is_buffer_acceptable(buffer, &rejection_reason)) {
            throw std::runtime_error("Cannot add child buffer: " + rejection_reason);
        }

        m_child_buffers.push_back(buffer);

        if (!buffer->get_processing_chain() && this->get_processing_chain()) {
            buffer->set_processing_chain(this->get_processing_chain());
        }
    }

    void remove_child_buffer_direct(std::shared_ptr<BufferType> buffer)
    {
        auto it = std::find(m_child_buffers.begin(), m_child_buffers.end(), buffer);
        if (it != m_child_buffers.end()) {
            m_child_buffers.erase(it);
        }
    }

    /**
     * @brief Process pending operations - call this at start of processing cycles
     */
    void process_pending_buffer_operations()
    {
        for (size_t i = 0; i < MAX_PENDING; ++i) {
            if (m_pending_ops[i].active.load(std::memory_order_acquire)) {
                auto& op = m_pending_ops[i];

                if (op.is_addition) {
                    add_child_buffer_direct(op.buffer);
                } else {
                    remove_child_buffer_direct(op.buffer);
                }

                // Clear operation
                op.buffer.reset();
                op.active.store(false, std::memory_order_release);
                m_pending_count.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Vector of tributary buffers that contribute to this root buffer
     */
    std::vector<std::shared_ptr<BufferType>> m_child_buffers;

    /**
     * @brief Processing rate hint for this buffer
     *
     * This is used to optimize processing based on expected
     * sample/frame rates, allowing the buffer to adapt its
     * processing strategy accordingly.
     */
    uint32_t m_processing_rate_hint;

    /**
     * @brief Whether this buffer allows cross-modal data sharing
     *
     * When enabled, the buffer can be accessed by different
     * subsystems simultaneously, allowing advanced cross-modal
     * processing techniques.
     */
    bool m_cross_modal_sharing;

    /**
     * @brief Current token enforcement strategy for this root buffer
     *
     * This defines how child buffers are validated and processed
     * based on their processing tokens. It allows for flexible
     * control over how different processing streams interact.
     */
    TokenEnforcementStrategy m_token_enforcement_strategy { TokenEnforcementStrategy::STRICT };

    /**
     * @brief Preferred processing token for this root buffer
     *
     * This is the token that child buffers should ideally match
     * to be accepted into the aggregation hierarchy. It defines
     * the primary processing stream for this root buffer.
     */
    ProcessingToken m_preferred_processing_token;

    std::atomic<uint32_t> m_pending_count { 0 };

    static constexpr size_t MAX_PENDING = 64;

    /**
     * @brief Structure for storing pending buffer add/remove operations
     *
     * Similar to RootNode's PendingOp, this handles buffer operations
     * that need to be deferred when the buffer is currently processing.
     */
    struct PendingBufferOp {
        std::atomic<bool> active { false };
        std::shared_ptr<BufferType> buffer;
        bool is_addition { true }; // true = add, false = remove
    } m_pending_ops[MAX_PENDING];
};
}
