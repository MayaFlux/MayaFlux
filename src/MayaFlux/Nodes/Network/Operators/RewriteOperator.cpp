#include "RewriteOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <charconv>

namespace MayaFlux::Nodes::Network {

RewriteOperator::Symbol::Symbol(SymbolId symbol_id, std::initializer_list<double> values)
    : id(symbol_id)
{
    const auto count = std::min(values.size(), max_symbol_parameters);
    std::ranges::copy_n(values.begin(), count, parameters.begin());

    const auto stored_arity = values.size() > max_symbol_parameters
        ? max_symbol_parameters + 1
        : count;

    arity = static_cast<uint8_t>(stored_arity);
}

std::optional<double> RewriteOperator::Symbol::parameter(size_t index) const noexcept
{
    return arity <= max_symbol_parameters && index < arity
        ? std::optional<double> { parameters[index] }
        : std::nullopt;
}

RewriteOperator::RewriteOperator(Config config)
    : m_max_word_length(config.max_word_length)
    , m_max_generations(config.max_generations)
    , m_valid(true)
    , m_coefficients(config.coefficient_count)
    , m_ignore(std::move(config.ignore))
{
    for (auto& coefficient : m_coefficients) {
        coefficient.store(0.0, std::memory_order_relaxed);
    }

    if (config.axiom.size() > m_max_word_length) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Configuration,
            "RewriteOperator axiom has {} symbols, exceeding max_word_length {}",
            config.axiom.size(), m_max_word_length);
        m_valid = false;
    }

    if (std::ranges::any_of(config.axiom, [](const Symbol& symbol) {
            return symbol.arity > max_symbol_parameters;
        })) {
        MF_ERROR(Journal::Component::Nodes, Journal::Context::Configuration,
            "RewriteOperator axiom contains a symbol with more than {} parameters",
            max_symbol_parameters);
        m_valid = false;
    }

    m_axiom = std::make_shared<const Word>(Word {
        .symbols = m_valid ? std::move(config.axiom) : std::vector<Symbol> {},
        .generation = 0 });
    m_draft = m_axiom;
    m_active_word = m_axiom;
    m_submitted = Submission { .word = m_axiom, .id = 0 };
    m_active = Submission { .word = m_axiom, .id = 0 };
}

std::vector<RewriteOperator::Symbol> RewriteOperator::symbols_from_text(std::string_view text)
{
    std::vector<Symbol> symbols;
    symbols.reserve(text.size());
    for (const char character : text) {
        symbols.emplace_back(static_cast<SymbolId>(static_cast<unsigned char>(character)));
    }
    return symbols;
}

bool RewriteOperator::replace(Replacement replacement)
{
    if (!m_valid) {
        return false;
    }
    if (!replacement.successor) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "RewriteOperator ignored replacement without a successor");
        return false;
    }

    m_replacements.push_back(std::move(replacement));
    return true;
}

bool RewriteOperator::replace(SymbolId predecessor, Successor successor, Predicate predicate)
{
    return replace(Replacement {
        .predecessor = predecessor,
        .predicate = std::move(predicate),
        .successor = std::move(successor) });
}

RewriteOperator::Successor RewriteOperator::text_successor(std::string_view text)
{
    return [symbols = symbols_from_text(text)](const Symbol&, Environment, const EmitSymbol& emit) {
        for (const auto& symbol : symbols) {
            if (!emit(symbol)) {
                return;
            }
        }
    };
}

bool RewriteOperator::replace(SymbolId predecessor, std::string_view successor, double weight)
{
    return replace(Replacement {
        .predecessor = predecessor,
        .weight = weight,
        .successor = text_successor(successor) });
}

bool RewriteOperator::matches_context(const Replacement& replacement, const Environment& environment) const
{
    const auto ignored = [this](const Symbol& symbol) {
        return std::ranges::find(m_ignore, symbol.id) != m_ignore.end();
    };

    const auto word = environment.word;
    size_t cursor = environment.position;
    for (const auto expected : replacement.left | std::views::reverse) {
        do {
            if (cursor == 0) {
                return false;
            }
            --cursor;
        } while (ignored(word[cursor]));

        if (word[cursor].id != expected) {
            return false;
        }
    }

    cursor = environment.position;
    for (const auto expected : replacement.right) {
        do {
            ++cursor;
            if (cursor >= word.size()) {
                return false;
            }
        } while (ignored(word[cursor]));

        if (word[cursor].id != expected) {
            return false;
        }
    }

    return true;
}

const RewriteOperator::Replacement* RewriteOperator::select_replacement(const Symbol& symbol, const Environment& environment)
{
    m_candidates.clear();
    m_weights.clear();

    for (const auto& replacement : m_replacements) {
        if (replacement.predecessor != symbol.id
            || !matches_context(replacement, environment)
            || (replacement.predicate && !replacement.predicate(symbol, environment))) {
            continue;
        }

        if (!(replacement.weight > 0.0)) {
            if (m_candidates.empty()) {
                return &replacement;
            }
            continue;
        }

        m_candidates.push_back(&replacement);
        m_weights.push_back(replacement.weight);
    }

    if (m_candidates.empty()) {
        return nullptr;
    }
    if (m_candidates.size() == 1) {
        return m_candidates.front();
    }

    const auto pick = Kinesis::Stochastic::Weights::weighted_pick(m_weights, m_generator);
    return m_candidates[std::min(pick, m_candidates.size() - 1)];
}

bool RewriteOperator::set_coefficient(size_t index, double value)
{
    if (index >= m_coefficients.size()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "RewriteOperator coefficient index {} exceeds {}", index, m_coefficients.size());
        return false;
    }

    m_coefficients[index].store(value, std::memory_order_release);
    return true;
}

std::optional<double> RewriteOperator::coefficient(size_t index) const noexcept
{
    return index < m_coefficients.size()
        ? std::optional<double> { m_coefficients[index].load(std::memory_order_acquire) }
        : std::nullopt;
}

std::optional<size_t> RewriteOperator::coefficient_index(std::string_view name) const noexcept
{
    if (name.size() < 2 || name.front() != 'c') {
        return std::nullopt;
    }

    size_t index {};
    const auto digits = name.substr(1);
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), index);
    if (error != std::errc {} || end != digits.data() + digits.size() || index >= m_coefficients.size()) {
        return std::nullopt;
    }
    return index;
}

void RewriteOperator::set_parameter(std::string_view name, double value)
{
    if (const auto index = coefficient_index(name)) {
        set_indexed_parameter(*index, value);
    } else {
        MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
            "RewriteOperator ignored unknown parameter '{}'", name);
    }
}

std::optional<double> RewriteOperator::query_state(std::string_view name) const
{
    if (const auto index = coefficient_index(name)) {
        return coefficient(*index);
    }
    return std::nullopt;
}

std::optional<size_t> RewriteOperator::mapped_parameter_index(std::string_view name) const
{
    if (const auto index = coefficient_index(name)) {
        return index;
    }

    MF_WARN(Journal::Component::Nodes, Journal::Context::Configuration,
        "RewriteOperator ignored mapping for unknown parameter '{}'", name);
    return m_coefficients.size();
}

void RewriteOperator::set_indexed_parameter(size_t index, double value)
{
    if (index < m_coefficients.size()) {
        m_coefficients[index].store(value, std::memory_order_release);
    }
}

bool RewriteOperator::SubmissionMailbox::push(Submission& submission) noexcept
{
    if (m_full.load(std::memory_order_acquire)) {
        return false;
    }

    m_submission = std::move(submission);
    m_full.store(true, std::memory_order_release);
    return true;
}

RewriteOperator::Submission RewriteOperator::SubmissionMailbox::pop() noexcept
{
    if (!m_full.load(std::memory_order_acquire)) {
        return {};
    }

    auto submission = std::move(m_submission);
    m_full.store(false, std::memory_order_release);
    return submission;
}

void RewriteOperator::drain_retired()
{
    if (auto retired = m_retired.pop(); retired.word) {
        m_retirement_received = true;
    }

    if (m_pending && m_retirement_received
        && m_active_submission_id.load(std::memory_order_acquire) == m_submitted.id) {
        m_active_word = m_submitted.word;
        m_pending = false;
        m_retirement_received = false;
    }
}

RewriteOperator::RewriteResult RewriteOperator::rewrite(uint64_t steps, std::span<const double> inputs)
{
    if (!m_valid) {
        return RewriteResult::INVALID_CONFIG;
    }
    if (steps > m_max_generations - m_draft->generation) {
        return RewriteResult::GENERATION_LIMIT;
    }
    if (steps == 0) {
        return RewriteResult::REWRITTEN;
    }

    try {
        std::vector<double> coefficients(m_coefficients.size());
        for (size_t index = 0; index < coefficients.size(); ++index) {
            coefficients[index] = m_coefficients[index].load(std::memory_order_acquire);
        }
        auto current = m_draft;

        for (uint64_t step = 0; step < steps; ++step) {
            const auto& [symbols, generation] = *current;
            std::vector<Symbol> next_symbols;
            next_symbols.reserve(std::min(m_max_word_length, symbols.size()));
            bool overflow {};

            const EmitSymbol emit = [this, &next_symbols, &overflow](Symbol next) {
                if (next.arity > max_symbol_parameters || next_symbols.size() >= m_max_word_length) {
                    overflow = true;
                    return false;
                }
                next_symbols.push_back(next);
                return true;
            };

            for (size_t position = 0; position < symbols.size(); ++position) {
                const auto& symbol = symbols[position];
                const Environment environment {
                    .coefficients = coefficients,
                    .inputs = inputs,
                    .word = symbols,
                    .position = position
                };

                if (const auto* replacement = select_replacement(symbol, environment)) {
                    replacement->successor(symbol, environment, emit);
                } else {
                    emit(symbol);
                }

                if (overflow) {
                    MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
                        "RewriteOperator generation exceeds max_word_length or symbol parameter limit");
                    return RewriteResult::WORD_LIMIT;
                }
            }

            current = std::make_shared<const Word>(Word {
                .symbols = std::move(next_symbols),
                .generation = generation + 1 });
        }

        m_draft = std::move(current);
        return RewriteResult::REWRITTEN;

    } catch (const std::exception& exception) {

        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "RewriteOperator replacement failed: {}", exception.what());
    } catch (...) {

        MF_ERROR(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "RewriteOperator replacement failed with an unknown exception");
    }

    return RewriteResult::REPLACEMENT_FAILED;
}

RewriteOperator::SubmitResult RewriteOperator::submit()
{
    if (!m_valid) {
        return SubmitResult::INVALID_CONFIG;
    }

    drain_retired();
    if (m_pending) {
        return SubmitResult::BUSY;
    }
    if (m_next_submission_id == std::numeric_limits<uint64_t>::max()) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "RewriteOperator submission ID exhausted");
        return SubmitResult::SUBMISSION_LIMIT;
    }

    Submission submission { .word = m_draft, .id = m_next_submission_id + 1 };
    if (!m_ready.push(submission)) {
        return SubmitResult::BUSY;
    }

    m_next_submission_id += 1;
    m_submitted = Submission { .word = m_draft, .id = m_next_submission_id };
    m_pending = true;
    return SubmitResult::SUBMITTED;
}

RewriteOperator::GrowResult RewriteOperator::grow(uint64_t steps, std::span<const double> inputs)
{
    if (!m_valid) {
        return GrowResult::INVALID_CONFIG;
    }

    drain_retired();
    if (m_pending) {
        return GrowResult::BUSY;
    }

    switch (rewrite(steps, inputs)) {
    case RewriteResult::REWRITTEN:
        break;
    case RewriteResult::INVALID_CONFIG:
        return GrowResult::INVALID_CONFIG;
    case RewriteResult::GENERATION_LIMIT:
        return GrowResult::GENERATION_LIMIT;
    case RewriteResult::WORD_LIMIT:
        return GrowResult::WORD_LIMIT;
    case RewriteResult::REPLACEMENT_FAILED:
        return GrowResult::REPLACEMENT_FAILED;
    }

    switch (submit()) {
    case SubmitResult::SUBMITTED:
        return GrowResult::GROWN;
    case SubmitResult::BUSY:
        return GrowResult::BUSY;
    case SubmitResult::SUBMISSION_LIMIT:
        return GrowResult::SUBMISSION_LIMIT;
    case SubmitResult::INVALID_CONFIG:
        break;
    }
    return GrowResult::INVALID_CONFIG;
}

bool RewriteOperator::published() const noexcept
{
    return m_draft == m_submitted.word
        && m_active_submission_id.load(std::memory_order_acquire) == m_submitted.id;
}

bool RewriteOperator::restore_axiom()
{
    if (!m_valid) {
        return false;
    }

    drain_retired();
    m_draft = m_axiom;
    return true;
}

std::shared_ptr<const RewriteOperator::Word> RewriteOperator::active_word() const
{
    const auto id = m_active_submission_id.load(std::memory_order_acquire);
    if (m_submitted.word && m_submitted.id == id) {
        return m_submitted.word;
    }
    return m_active_word;
}

void RewriteOperator::process(float)
{
    auto next = std::move(m_deferred);
    if (!next.word) {
        next = m_ready.pop();
        if (!next.word) {
            return;
        }
    }

    if (!m_retired.push(m_active)) {
        m_deferred = std::move(next);
        return;
    }

    m_active = std::move(next);
    m_active_generation.store(m_active.word->generation, std::memory_order_release);
    m_active_submission_id.store(m_active.id, std::memory_order_release);
}

} // namespace MayaFlux::Nodes::Network
