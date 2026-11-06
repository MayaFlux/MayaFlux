#pragma once

#include "GrammarHelper.hpp"
#include "MayaFlux/Yantra/ComputeOperation.hpp"

namespace MayaFlux::Yantra {

/** @brief Explicitly owned registrations connecting enum vocabularies, configs, and implementations. */
class OperationRegistry {
public:
    /** @brief A constructed implementation with its erased input and execution contracts. */
    struct Binding {
        std::type_index type { typeid(void) };
        Matcher::MatcherFunc matcher;
        std::function<std::any(const std::any&, const ExecutionContext&)> executor;
        int priority { 50 };
    };

    /** @brief Catalog an implementation for an enum vocabulary. A factory may supply its dependencies. */
    template <typename E, typename Op, typename Config = default_config_t<Op>>
        requires std::is_enum_v<E>
    OperationRegistry& catalog(
        std::type_identity_t<std::function<std::shared_ptr<Op>(Config)>> factory = {}, int priority = 50)
    {
        return catalog_impl<E, Op, Config>(std::nullopt, std::move(factory), priority);
    }

    /** @brief Catalog an implementation for one selection within an enum vocabulary. */
    template <typename Op, typename E, typename Config = default_config_t<Op>>
        requires std::is_enum_v<E>
    OperationRegistry& catalog(E selection,
        std::type_identity_t<std::function<std::shared_ptr<Op>(Config)>> factory = {}, int priority = 50)
    {
        return catalog_impl<E, Op, Config>(selection, std::move(factory), priority);
    }

    /** @brief Construct all registered implementations accepting this signature and config type. */
    [[nodiscard]] std::vector<Binding> resolve(
        const ComputationSignature& signature, const OperationConfig* config = nullptr) const
    {
        std::vector<Binding> bindings;
        for (const auto& registration : m_registrations) {
            if (auto binding = registration(signature, config)) {
                bindings.push_back(std::move(*binding));
            }
        }
        return bindings;
    }

private:
    template <typename E, typename Op, typename Config>
    OperationRegistry& catalog_impl(std::optional<E> only,
        std::function<std::shared_ptr<Op>(Config)> factory, int priority)
    {
        m_registrations.push_back(
            [only, factory = std::move(factory), priority](
                const ComputationSignature& signature, const OperationConfig* supplied) -> std::optional<Binding> {
                const auto selection = signature.template selection<E>(0);
                if (signature.size() != 1 || !selection || (only && *only != *selection)) {
                    return std::nullopt;
                }
                Config config {};
                if (supplied) {
                    if constexpr (std::same_as<Config, OperationConfig>) {
                        config = *supplied;
                    } else {
                        const auto* typed = supplied->template get<Config>();
                        if (!typed) {
                            return std::nullopt;
                        }
                        config = *typed;
                    }
                }
                if constexpr (requires { config.method = *selection; }
                    || requires { config.operation = *selection; }
                    || requires { config.algorithm = *selection; }) {
                    config = select_operation(*selection, std::move(config));
                } else if (!only) {
                    error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
                        std::source_location::current(),
                        "Catalog a single enum selection for an implementation without a method selector");
                }
                std::shared_ptr<Op> operation;
                if (factory) {
                    operation = factory(config);
                } else if constexpr (std::constructible_from<Op, Config>) {
                    operation = std::make_shared<Op>(config);
                } else if constexpr (std::default_initializable<Op>) {
                    operation = std::make_shared<Op>();
                } else {
                    error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
                        std::source_location::current(), "Catalog a factory for this operation");
                }
                if (!operation) {
                    error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::Runtime,
                        std::source_location::current(), "Catalog factory returned no operation");
                }
                if constexpr (requires { operation->set_config(config); }) {
                    operation->set_config(config);
                }
                using Input = std::remove_cvref_t<decltype(std::declval<typename Op::input_type>().data)>;
                using Output = std::remove_cvref_t<decltype(std::declval<typename Op::output_type>().data)>;
                std::shared_ptr<ComputeOperation<Input, Output>> base = std::move(operation);
                return Binding {
                    .type = std::type_index(typeid(Op)),
                    .matcher = [base](const std::any& input, const ExecutionContext&) {
                        const auto* typed = std::any_cast<typename Op::input_type>(&input);
                        return typed && base->validate_input(*typed);
                    },
                    .executor = [base](const std::any& input, const ExecutionContext& ctx) -> std::any {
                        apply_context_parameters(base, ctx);
                        base->set_last_execution_context(ctx);
                        return base->apply_operation(safe_any_cast_or_throw<typename Op::input_type>(input));
                    },
                    .priority = priority,
                };
            });
        return *this;
    }

    using Registration = std::function<std::optional<Binding>(const ComputationSignature&, const OperationConfig*)>;
    std::vector<Registration> m_registrations;
};

}
