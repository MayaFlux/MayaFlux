#pragma once

#include "NetworkOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class OperatorChain
 * @brief Ordered sequence of secondary operators applied after the primary operator.
 *
 * Mirrors the role of BufferProcessingChain relative to a Buffer's default processor.
 * NodeNetwork subclasses hold one primary operator (m_operator / create_operator<T>)
 * whose contract and external API remain unchanged. OperatorChain holds zero or more
 * additional operators that run in insertion order immediately after the primary.
 *
 * Ownership is shared: callers receive a shared_ptr<OpType> from emplace() and may
 * retain it for runtime reconfiguration (rebinding Tendency fields, adjusting
 * parameters) while the chain also holds a reference. Removal by pointer identity
 * is supported via remove().
 *
 * The chain has no knowledge of vertex layouts, domains, or which operator holds
 * final vertex state. Those concerns belong to the owning NodeNetwork subclass,
 * exposed via NodeNetwork::get_graphics_operator().
 *
 * Thread safety: not internally synchronised. All mutations must occur outside the
 * network's process_batch() call.
 */
class MAYAFLUX_API OperatorChain {
public:
    OperatorChain() = default;
    ~OperatorChain() = default;

    OperatorChain(const OperatorChain&) = delete;
    OperatorChain& operator=(const OperatorChain&) = delete;
    OperatorChain(OperatorChain&&) = default;
    OperatorChain& operator=(OperatorChain&&) = default;

    // -------------------------------------------------------------------------
    // Insertion
    // -------------------------------------------------------------------------

    /**
     * @brief Append an already-constructed operator to the chain.
     * @param op Operator to append. Must be non-null.
     */
    void add(std::shared_ptr<NetworkOperator> op);

    /**
     * @brief Construct an operator in-place and append it.
     * @tparam OpType Concrete operator type.
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to OpType constructor.
     * @return Shared pointer to the newly constructed operator.
     */
    template <typename OpType, typename... Args>
    std::shared_ptr<OpType> emplace(Args&&... args)
    {
        auto op = std::make_shared<OpType>(std::forward<Args>(args)...);
        m_operators.push_back(op);
        return op;
    }

    // -------------------------------------------------------------------------
    // Removal
    // -------------------------------------------------------------------------

    /**
     * @brief Remove a specific operator by pointer identity.
     * @param op Operator to remove. No-op if not found.
     */
    void remove(const std::shared_ptr<NetworkOperator>& op);

    /**
     * @brief Remove all operators from the chain.
     */
    void clear();

    // -------------------------------------------------------------------------
    // Processing
    // -------------------------------------------------------------------------

    /**
     * @brief Call process(dt) on each operator in insertion order.
     * @param dt Time delta or sample count passed through to each operator.
     */
    void process(float dt);

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------

    /**
     * @brief Returns true when the chain contains no operators.
     */
    [[nodiscard]] bool empty() const { return m_operators.empty(); }

    /**
     * @brief Number of operators currently in the chain.
     */
    [[nodiscard]] size_t size() const { return m_operators.size(); }

    /**
     * @brief Return the operator at the given index, or nullptr if out of range.
     * @param index Zero-based position in insertion order.
     */
    [[nodiscard]] std::shared_ptr<NetworkOperator> get(size_t index) const;

    /**
     * @brief Return the first operator whose dynamic type matches OpType, or nullptr.
     * @tparam OpType Type to search for.
     */
    template <typename OpType>
    [[nodiscard]] std::shared_ptr<OpType> find() const
    {
        for (const auto& op : m_operators) {
            if (auto cast = std::dynamic_pointer_cast<OpType>(op))
                return cast;
        }
        return nullptr;
    }

    /**
     * @brief Read-only access to the underlying operator vector.
     *
     * Used by NodeNetwork::get_graphics_operator() to scan the chain
     * without embedding domain assumptions inside OperatorChain itself.
     */
    [[nodiscard]] const std::vector<std::shared_ptr<NetworkOperator>>& operators() const
    {
        return m_operators;
    }

private:
    std::vector<std::shared_ptr<NetworkOperator>> m_operators;
};

} // namespace MayaFlux::Nodes::Network
