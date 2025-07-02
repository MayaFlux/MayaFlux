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
        std::lock_guard<std::mutex> lock(m_mutex);

        std::string rejection_reason;
        if (!is_buffer_acceptable(buffer, &rejection_reason)) {
            throw std::runtime_error("Cannot add child buffer: " + rejection_reason);
        }

        m_child_buffers.push_back(buffer);

        if (!buffer->get_processing_chain() && this->get_processing_chain()) {
            buffer->set_processing_chain(this->get_processing_chain());
        }
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
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!is_buffer_acceptable(buffer, rejection_reason)) {
            return false;
        }

        m_child_buffers.push_back(buffer);

        if (!buffer->get_processing_chain() && this->get_processing_chain()) {
            buffer->set_processing_chain(this->get_processing_chain());
        }

        return true;
    }

    /**
     * @brief Removes a tributary buffer from this root buffer
     * @param buffer Child buffer to remove from the aggregation hierarchy
     */
    virtual void remove_child_buffer(std::shared_ptr<BufferType> buffer)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find(m_child_buffers.begin(), m_child_buffers.end(), buffer);
        if (it != m_child_buffers.end()) {
            m_child_buffers.erase(it);
        }
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
    virtual void set_processing_rate_hint(u_int32_t tick_rate) { m_processing_rate_hint = tick_rate; }

    /**
     * @brief Gets the processing rate hint
     * @return Expected processing rate in samples/frames per second
     */
    virtual u_int32_t get_processing_rate_hint() const { return m_processing_rate_hint; }

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

protected:
    /**
     * @brief Vector of tributary buffers that contribute to this root buffer
     */
    std::vector<std::shared_ptr<BufferType>> m_child_buffers;

    /**
     * @brief Mutex for thread-safe access to shared data resources
     *
     * Protects access to the buffer data during processing and
     * when setting node output, ensuring thread safety between
     * real-time processing threads and other computational threads.
     */
    std::mutex m_mutex;

    /**
     * @brief Processing rate hint for this buffer
     *
     * This is used to optimize processing based on expected
     * sample/frame rates, allowing the buffer to adapt its
     * processing strategy accordingly.
     */
    u_int32_t m_processing_rate_hint;

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
};
}
