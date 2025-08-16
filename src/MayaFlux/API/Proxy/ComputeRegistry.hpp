#pragma once

#include "MayaFlux/Yantra/OperationSpec/ExecutionContext.hpp"

namespace MayaFlux::Yantra {

/**
 * @brief Macro to declare operation traits for automatic registration
 *
 * This macro should be used in operation classes to provide intrinsic
 * type information that the registry can use for automatic categorization.
 *
 * Usage:
 * class MyAnalyzer : public UniversalAnalyzer<...> {
 * public:
 *     DECLARE_OPERATION_TRAITS(ANALYZER)
 *     // ... rest of implementation
 * };
 */
#define DECLARE_OPERATION_TRAITS(CATEGORY)                                       \
    static constexpr OperationType operation_category = OperationType::CATEGORY; \
    using base_input_type = typename base_type::input_type;                      \
    using base_output_type = typename base_type::output_type;                    \
    static constexpr const char* operation_name = #CATEGORY;

/**
 * @class OperationRegistry
 * @brief Manages operation type registration, discovery, and factory creation
 *
 * The OperationRegistry acts as a centralized repository for all operation types
 * in the Yantra compute system. It provides:
 * - Type-safe registration with automatic trait detection
 * - Factory-based operation creation
 * - Discovery mechanisms for finding compatible operations
 * - Integration points with the broader API/Proxy/Creator system
 *
 * This registry follows a similar pattern to the channel/domain/shared_ptr model
 * but focuses on operation types rather than runtime instances.
 */
class OperationRegistry {
public:
    using Factory = std::function<std::any()>;

    /**
     * @brief Register an operation using its intrinsic traits
     * @tparam OpClass Operation class with DECLARE_OPERATION_TRAITS
     */
    template <typename OpClass>
    void register_operation()
    {
        static_assert(has_operation_traits<OpClass>::value,
            "Operation must have intrinsic type information. Use DECLARE_OPERATION_TRAITS macro.");

        OperationType category = OpClass::operation_category;
        register_operation<OpClass>(category);
    }

    /**
     * @brief Register with explicit category (for operations without traits)
     * @tparam OpClass Operation class to register
     * @param category Explicit operation category
     */
    template <typename OpClass>
    void register_operation(OperationType category)
    {
        TypeKey key {
            .category = category,
            .operation_type = std::type_index(typeid(OpClass))
        };

        m_factories[key] = []() -> std::any {
            return static_pointer_cast<void>(std::make_shared<OpClass>());
        };

        m_type_info[key] = {
            std::type_index(typeid(typename OpClass::input_type)),
            std::type_index(typeid(typename OpClass::output_type))
        };
    }

    /**
     * @brief Register with custom factory function
     * @tparam OpClass Operation class to register
     * @param category Operation category
     * @param factory Custom factory function
     */
    template <typename OpClass>
    void register_operation_with_factory(OperationType category,
        std::function<std::shared_ptr<OpClass>()> factory)
    {
        TypeKey key {
            .category = category,
            .operation_type = std::type_index(typeid(OpClass))
        };

        m_factories[key] = [factory]() -> std::any {
            return std::static_pointer_cast<void>(factory());
        };

        m_type_info[key] = {
            std::type_index(typeid(typename OpClass::input_type)),
            std::type_index(typeid(typename OpClass::output_type))
        };
    }

    /**
     * @brief Create an operation instance
     * @tparam OpClass Operation class to create
     * @return Shared pointer to new operation instance, or nullptr if not registered
     */
    template <typename OpClass>
    std::shared_ptr<OpClass> create()
    {
        if constexpr (has_operation_category<OpClass>::value) {
            TypeKey key { OpClass::operation_category, std::type_index(typeid(OpClass)) };
            auto it = m_factories.find(key);
            if (it != m_factories.end()) {
                return std::static_pointer_cast<OpClass>(
                    std::any_cast<std::shared_ptr<void>>(it->second()));
            }
        }

        for (const auto& [key, factory] : m_factories) {
            if (key.operation_type == std::type_index(typeid(OpClass))) {
                return std::static_pointer_cast<OpClass>(
                    std::any_cast<std::shared_ptr<void>>(factory()));
            }
        }

        return nullptr;
    }

    /**
     * @brief Check if an operation type is registered
     * @tparam OpClass Operation class to check
     * @return true if registered, false otherwise
     */
    template <typename OpClass>
    bool is_registered() const
    {
        return std::ranges::any_of(m_factories, [](const auto& pair) {
            const auto& key = pair.first;
            return key.operation_type == std::type_index(typeid(OpClass));
        });
    }

    /**
     * @brief Discover operations matching specific criteria
     * @param category Operation category to search
     * @param input_type Required input type
     * @param output_type Required output type
     * @return Vector of type indices for matching operations
     */
    std::vector<std::type_index> discover_operations(OperationType category,
        std::type_index input_type,
        std::type_index output_type) const
    {
        std::vector<std::type_index> results;

        for (const auto& [key, type_info] : m_type_info) {
            if (key.category == category && type_info.first == input_type && type_info.second == output_type) {
                results.push_back(key.operation_type);
            }
        }

        return results;
    }

    /**
     * @brief Get all registered operations of a specific category
     * @param category Category to query
     * @return Vector of type indices for operations in that category
     */
    std::vector<std::type_index> get_operations_by_category(OperationType category) const
    {
        std::vector<std::type_index> results;

        for (const auto& [key, _] : m_factories) {
            if (key.category == category) {
                results.push_back(key.operation_type);
            }
        }

        return results;
    }

    /**
     * @brief Get category for a registered operation type
     * @tparam OpClass Operation class to query
     * @return Optional containing the category if registered
     */
    template <typename OpClass>
    std::optional<OperationType> get_category() const
    {
        auto type_idx = std::type_index(typeid(OpClass));

        for (const auto& [key, _] : m_factories) {
            if (key.operation_type == type_idx) {
                return key.category;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Clear all registrations
     */
    void clear()
    {
        m_factories.clear();
        m_type_info.clear();
    }

    /**
     * @brief Get total number of registered operations
     */
    size_t size() const
    {
        return m_factories.size();
    }

private:
    struct TypeKey {
        OperationType category;
        std::type_index operation_type;

        bool operator==(const TypeKey& other) const
        {
            return category == other.category && operation_type == other.operation_type;
        }
    };

    struct TypeKeyHash {
        std::size_t operator()(const TypeKey& key) const
        {
            return std::hash<int>()(static_cast<int>(key.category)) ^ std::hash<std::type_index>()(key.operation_type);
        }
    };

    std::unordered_map<TypeKey, Factory, TypeKeyHash> m_factories;
    std::unordered_map<TypeKey, std::pair<std::type_index, std::type_index>, TypeKeyHash> m_type_info;

    template <typename T, typename = void>
    struct has_operation_category : std::false_type { };

    template <typename T>
    struct has_operation_category<T, std::void_t<decltype(T::operation_category)>>
        : std::true_type { };

    template <typename T, typename = void>
    struct has_operation_traits : std::false_type { };

    template <typename T>
    struct has_operation_traits<T, std::void_t<decltype(T::operation_category), typename T::input_type, typename T::output_type>> : std::true_type { };
};

/**
 * @brief Global operation registry accessor
 * Similar to how Engine provides global access to core systems
 */
inline std::shared_ptr<OperationRegistry> get_operation_registry()
{
    static auto registry = std::make_shared<OperationRegistry>();
    return registry;
}

/**
 * @brief Automatic registration helper
 * Operations can use this in their implementation files for auto-registration
 */
template <typename OpClass>
struct AutoRegisterOperation {
    AutoRegisterOperation()
    {
        get_operation_registry()->register_operation<OpClass>();
    }
};

/**
 * @brief Macro for automatic registration in implementation files
 * Usage: REGISTER_OPERATION(MyAnalyzer);
 */
#define REGISTER_OPERATION(OpClass) \
    static MayaFlux::Yantra::AutoRegisterOperation<OpClass> _auto_register_##OpClass;

} // namespace MayaFlux::Yantra
