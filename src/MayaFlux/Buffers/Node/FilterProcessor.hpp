#pragma once

#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Nodes::Filters {
class Filter;
class FIR;
class IIR;
} // namespace MayaFlux::Nodes::Filters

namespace MayaFlux::Buffers {

/**
 * @class FilterProcessor
 * @brief Buffer processor that applies filter nodes to audio data
 *
 * This processor connects a Filter node (FIR or IIR) to an AudioBuffer,
 * allowing filtering operations to be applied to buffer data. It supports
 * both internally managed filter nodes and externally provided filter nodes.
 */
class MAYAFLUX_API FilterProcessor : public BufferProcessor {
public:
    FilterProcessor() = default;

    /**
     * @brief Creates processor with FIR filter
     */
    template <typename... Args>
        requires std::constructible_from<Nodes::Filters::FIR, Args...>
    FilterProcessor(Args&&... args)
        : m_filter(std::make_shared<Nodes::Filters::FIR>(std::forward<Args>(args)...))
        , m_use_internal(true)
    {
    }

    /**
     * @brief Creates processor with IIR filter
     */
    template <typename... Args>
        requires std::constructible_from<Nodes::Filters::IIR, Args...>
    FilterProcessor(Args&&... args)
        : m_filter(std::make_shared<Nodes::Filters::IIR>(std::forward<Args>(args)...))
        , m_use_internal(true)
    {
    }

    /**
     * @brief Creates processor with external filter node
     */
    FilterProcessor(const std::shared_ptr<Nodes::Filters::Filter>& filter)
        : m_filter(filter)
    {
    }

    void processing_function(std::shared_ptr<Buffer> buffer) override;

    void on_attach(std::shared_ptr<Buffer> buffer) override;

    [[nodiscard]] inline std::shared_ptr<Nodes::Filters::Filter> get_filter() const
    {
        return m_filter;
    }

    [[nodiscard]] inline bool is_using_internal() const { return m_use_internal; }

    inline void update_filter_node(const std::shared_ptr<Nodes::Filters::Filter>& filter)
    {
        m_pending_filter = filter;
    }

private:
    std::shared_ptr<Nodes::Filters::Filter> m_filter;
    std::shared_ptr<Nodes::Filters::Filter> m_pending_filter;
    bool m_use_internal {};
    size_t m_current_position = 0;

    void process_single_sample(double& sample);
};

} // namespace MayaFlux::Buffers
