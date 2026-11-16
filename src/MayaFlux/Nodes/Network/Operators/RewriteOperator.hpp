#pragma once

#include "RelationOperators.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @brief Bounded parallel rewriting of a typed word in a RelationNetwork.
 *
 * A symbol is an id plus up to eight numeric parameters. A character literal
 * is its own id, so grammars read the way they are written on paper.
 * Replacements may be conditional, context sensitive and stochastic.
 *
 * Rewriting runs on one producer lane and builds a draft word. submit() hands
 * the draft to the network, and process() makes it the active word at a block
 * boundary without allocating or releasing word storage.
 *
 * @code
 * auto rewrite = n->add<RewriteOperator>(RewriteOperator::Config {
 *     .axiom = RewriteOperator::symbols_from_text("F"),
 *     .max_word_length = 4096,
 *     .max_generations = 5 });
 * rewrite->replace('F', "F[+F]F[-F]F");
 * rewrite->rewrite(4);
 * rewrite->submit();
 * @endcode
 */
class MAYAFLUX_API RewriteOperator final : public RelationOperator {
public:
    using SymbolId = uint32_t;
    static constexpr size_t max_symbol_parameters = 8;

    struct Symbol {
        SymbolId id {};
        std::array<double, max_symbol_parameters> parameters {};
        uint8_t arity {};

        Symbol(SymbolId id, std::initializer_list<double> values = {});

        template <std::convertible_to<double>... Values>
            requires(sizeof...(Values) > 0 && sizeof...(Values) <= max_symbol_parameters)
        Symbol(SymbolId symbol_id, Values... values)
            : id(symbol_id)
            , parameters { static_cast<double>(values)... }
            , arity(static_cast<uint8_t>(sizeof...(Values)))
        {
        }

        [[nodiscard]] std::optional<double> parameter(size_t index) const noexcept;
    };

    struct Word {
        std::vector<Symbol> symbols;
        uint64_t generation {};
    };

    /** @brief What a replacement sees: the coefficients, the caller's inputs, and the word with the symbol's position in it. */
    struct Environment {
        std::span<const double> coefficients;
        std::span<const double> inputs;
        std::span<const Symbol> word;
        size_t position {};
    };

    /** @brief Append a symbol; false on a bound violation. Valid only during the replacement call. */
    using EmitSymbol = std::function<bool(Symbol)>;
    using Predicate = std::function<bool(const Symbol&, Environment)>;
    using Successor = std::function<void(const Symbol&, Environment, const EmitSymbol&)>;

    /**
     * @brief One rewrite rule: left < predecessor > right, if predicate, with weight
     *
     * left and right are matched against the symbols adjacent to the
     * predecessor in word order, skipping Config::ignore. The first matching
     * replacement wins, unless it has a positive weight: then one of the
     * matching weighted replacements of that symbol is drawn in proportion to
     * its weight, which is the stochastic L-system.
     */
    struct Replacement {
        SymbolId predecessor {};
        std::vector<SymbolId> left;
        std::vector<SymbolId> right;
        Predicate predicate;
        double weight {};
        Successor successor;
    };

    struct Config {
        std::vector<Symbol> axiom;
        size_t max_word_length {};
        uint64_t max_generations {};
        size_t coefficient_count {};
        std::vector<SymbolId> ignore;
    };

    enum class RewriteResult : uint8_t {
        REWRITTEN,
        INVALID_CONFIG,
        GENERATION_LIMIT,
        WORD_LIMIT,
        REPLACEMENT_FAILED
    };

    enum class SubmitResult : uint8_t {
        SUBMITTED,
        INVALID_CONFIG,
        BUSY,
        SUBMISSION_LIMIT
    };

    explicit RewriteOperator(Config config);

    [[nodiscard]] bool valid() const noexcept { return m_valid; }

    /** @brief One parameterless symbol per character, each character being its own id. */
    [[nodiscard]] static std::vector<Symbol> symbols_from_text(std::string_view text);

    /** @brief A successor that emits the text's characters as parameterless symbols. */
    [[nodiscard]] static Successor text_successor(std::string_view text);

    /** @brief Append a rule; false when the operator is invalid or the successor is empty. */
    bool replace(Replacement replacement);
    bool replace(SymbolId predecessor, Successor successor, Predicate predicate = {});

    /** @brief Append a rule whose successor is text. A positive weight makes it one choice of a stochastic rule. */
    bool replace(SymbolId predecessor, std::string_view successor, double weight = 0.0);
    void clear_replacements() noexcept { m_replacements.clear(); }

    /**
     * @brief The generator behind weighted choices
     * @note Producer lane only. Seed it for a repeatable word.
     */
    [[nodiscard]] Kinesis::Stochastic::Stochastic& generator() noexcept { return m_generator; }

    /** @brief Set one coefficient. map() addresses it as "c0", "c1", ... */
    bool set_coefficient(size_t index, double value);
    [[nodiscard]] std::optional<double> coefficient(size_t index) const noexcept;

    /** @brief Rewrite the draft word on the producer lane, without submitting it. */
    [[nodiscard]] RewriteResult rewrite(uint64_t steps = 1, std::span<const double> inputs = {});

    /** @brief Submit the draft word; it becomes the active word at the next RelationNetwork block. */
    [[nodiscard]] SubmitResult submit();

    /** @brief Restore the axiom as the draft word on the producer lane, without submitting it. */
    [[nodiscard]] bool restore_axiom();

    [[nodiscard]] uint64_t draft_generation() const noexcept { return m_draft->generation; }

    /**
     * @brief The draft word on the producer lane: the axiom, or what rewrite() last built.
     *
     * It is the word submit() would hand over, so a producer can read what it
     * has just rewritten before submitting it. A rewrite that a bound refused
     * leaves it unchanged. The word is immutable and stays valid after it is
     * submitted or replaced.
     */
    [[nodiscard]] std::shared_ptr<const Word> draft_word() const noexcept { return m_draft; }
    [[nodiscard]] uint64_t active_generation() const noexcept
    {
        return m_active_generation.load(std::memory_order_acquire);
    }

    /** @brief Return the latest active word on the producer lane. */
    [[nodiscard]] std::shared_ptr<const Word> active_word() const;

    /** @brief Read the active word on the RelationNetwork processing lane. */
    [[nodiscard]] std::span<const Symbol> active_symbols() const noexcept
    {
        return m_active.word->symbols;
    }

    void process(float frames) override;

    /** @brief Set "c0", "c1", ...; unknown names are logged and ignored. */
    void set_parameter(std::string_view name, double value) override;

    /** @brief Return a named coefficient, or nullopt for an unknown name. */
    [[nodiscard]] std::optional<double> query_state(std::string_view name) const override;
    [[nodiscard]] std::string_view get_type_name() const override { return "Rewrite"; }

protected:
    [[nodiscard]] std::optional<size_t> mapped_parameter_index(std::string_view name) const override;
    void set_indexed_parameter(size_t index, double value) override;

private:
    struct Submission {
        std::shared_ptr<const Word> word;
        uint64_t id {};
    };

    class SubmissionMailbox {
    public:
        [[nodiscard]] bool push(Submission& submission) noexcept;
        [[nodiscard]] Submission pop() noexcept;

    private:
        Submission m_submission;
        std::atomic<bool> m_full { false };
    };

    void drain_retired();
    [[nodiscard]] std::optional<size_t> coefficient_index(std::string_view name) const noexcept;
    [[nodiscard]] bool matches_context(const Replacement& replacement, const Environment& environment) const;
    [[nodiscard]] const Replacement* select_replacement(const Symbol& symbol, const Environment& environment);

    std::shared_ptr<const Word> m_axiom;
    size_t m_max_word_length;
    uint64_t m_max_generations;
    bool m_valid;
    std::vector<std::atomic<double>> m_coefficients;
    std::vector<SymbolId> m_ignore;
    std::vector<Replacement> m_replacements;
    Kinesis::Stochastic::Stochastic m_generator;
    std::vector<const Replacement*> m_candidates;
    std::vector<double> m_weights;

    std::shared_ptr<const Word> m_draft;
    std::shared_ptr<const Word> m_active_word;
    Submission m_submitted;
    uint64_t m_next_submission_id {};
    bool m_pending {};
    bool m_retirement_received {};

    SubmissionMailbox m_ready;
    SubmissionMailbox m_retired;
    Submission m_active;
    Submission m_deferred;
    std::atomic<uint64_t> m_active_generation;
    std::atomic<uint64_t> m_active_submission_id;
};

} // namespace MayaFlux::Nodes::Network
