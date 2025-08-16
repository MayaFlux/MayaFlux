#pragma once

#include <typeindex>

namespace MayaFlux::Yantra {

/**
 * @struct PooledOperationInfo
 * @brief Metadata about a pooled operation
 */
struct PooledOperationInfo {
    std::string name;
    std::type_index type;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_accessed;
    size_t access_count = 0;
    std::unordered_map<std::string, std::any> metadata;

    PooledOperationInfo()
        : type(typeid(void))
    {
    }

    PooledOperationInfo(std::type_index t)
        : type(t)
    {
    }
};

/**
 * @class OperationPool
 * @brief Thread-safe pool for managing named operation instances
 *
 * The OperationPool provides efficient storage and retrieval of operation
 * instances by name. It includes thread safety, access tracking, and
 * advanced query capabilities.
 *
 * Key Features:
 * - Type-safe storage and retrieval
 * - Thread-safe operations with reader/writer locks
 * - Access tracking and statistics
 * - Bulk operations for efficiency
 * - Query and filtering capabilities
 * - Operation lifecycle callbacks
 */
class OperationPool {
public:
    using OperationPtr = std::shared_ptr<void>;
    using TypePredicate = std::function<bool(std::type_index)>;
    using NamePredicate = std::function<bool(const std::string&)>;

    /**
     * @brief Default constructor
     */
    OperationPool() = default;

    /**
     * @brief Add named operation to pool
     * @tparam OpClass Operation class type
     * @param name Unique name for the operation
     * @param op Shared pointer to the operation
     * @return true if added successfully, false if name already exists
     */
    template <typename OpClass>
    bool add(const std::string& name, std::shared_ptr<OpClass> op)
    {
        if (!op) {
            return false;
        }

        std::unique_lock lock(m_mutex);

        if (m_operations.contains(name)) {
            return false;
        }

        m_operations[name] = std::static_pointer_cast<void>(op);

        PooledOperationInfo info;
        info.name = name;
        info.type = std::type_index(typeid(OpClass));
        info.created_at = std::chrono::steady_clock::now();
        info.last_accessed = info.created_at;
        info.access_count = 0;

        m_info[name] = std::move(info);

        if (m_on_add_callback) {
            m_on_add_callback(name, info.type);
        }

        return true;
    }

    /**
     * @brief Add or replace operation in pool
     * @tparam OpClass Operation class type
     * @param name Name for the operation
     * @param op Shared pointer to the operation
     */
    template <typename OpClass>
    void set(const std::string& name, std::shared_ptr<OpClass> op)
    {
        if (!op) {
            return;
        }

        std::unique_lock lock(m_mutex);

        bool replacing = m_operations.contains(name);

        m_operations[name] = std::static_pointer_cast<void>(op);

        auto& info = m_info[name];
        info.name = name;
        info.type = std::type_index(typeid(OpClass));

        if (!replacing) {
            info.created_at = std::chrono::steady_clock::now();
            info.access_count = 0;
        }
        info.last_accessed = std::chrono::steady_clock::now();

        if (replacing && m_on_replace_callback) {
            m_on_replace_callback(name, info.type);
        } else if (!replacing && m_on_add_callback) {
            m_on_add_callback(name, info.type);
        }
    }

    /**
     * @brief Get operation from pool with type safety
     * @tparam OpClass Expected operation class type
     * @param name Name of the operation
     * @return Shared pointer to operation, or nullptr if not found/type mismatch
     */
    template <typename OpClass>
    std::shared_ptr<OpClass> get(const std::string& name)
    {
        std::shared_lock lock(m_mutex);

        auto it = m_operations.find(name);
        if (it == m_operations.end()) {
            return nullptr;
        }

        auto info_it = m_info.find(name);
        if (info_it != m_info.end()) {
            if (info_it->second.type != std::type_index(typeid(OpClass))) {
                return nullptr;
            }

            {
                lock.unlock();
                std::unique_lock write_lock(m_mutex);
                info_it->second.last_accessed = std::chrono::steady_clock::now();
                info_it->second.access_count++;
            }
        }

        return std::static_pointer_cast<OpClass>(it->second);
    }

    /**
     * @brief Try to get operation, return optional
     * @tparam OpClass Expected operation class type
     * @param name Name of the operation
     * @return Optional containing the operation if found and correct type
     */
    template <typename OpClass>
    std::optional<std::shared_ptr<OpClass>> try_get(const std::string& name)
    {
        auto op = get<OpClass>(name);
        return op ? std::optional(op) : std::nullopt;
    }

    /**
     * @brief Remove operation from pool
     * @param name Name of the operation to remove
     * @return true if removed, false if not found
     */
    bool remove(const std::string& name)
    {
        std::unique_lock lock(m_mutex);

        auto it = m_operations.find(name);
        if (it == m_operations.end()) {
            return false;
        }

        std::type_index type = m_info[name].type;

        m_operations.erase(it);
        m_info.erase(name);

        if (m_on_remove_callback) {
            m_on_remove_callback(name, type);
        }

        return true;
    }

    /**
     * @brief Remove all operations of a specific type
     * @tparam OpClass Operation class type to remove
     * @return Number of operations removed
     */
    template <typename OpClass>
    size_t remove_by_type()
    {
        std::unique_lock lock(m_mutex);

        std::type_index target_type(typeid(OpClass));
        std::vector<std::string> to_remove;

        for (const auto& [name, info] : m_info) {
            if (info.type == target_type) {
                to_remove.push_back(name);
            }
        }

        for (const auto& name : to_remove) {
            m_operations.erase(name);
            m_info.erase(name);

            if (m_on_remove_callback) {
                m_on_remove_callback(name, target_type);
            }
        }

        return to_remove.size();
    }

    /**
     * @brief Clear all operations from the pool
     */
    void clear()
    {
        std::unique_lock lock(m_mutex);
        m_operations.clear();
        m_info.clear();
    }

    /**
     * @brief List all operation names
     * @return Vector of operation names
     */
    std::vector<std::string> list_names() const
    {
        std::shared_lock lock(m_mutex);

        std::vector<std::string> names;
        names.reserve(m_operations.size());

        for (const auto& [name, _] : m_operations) {
            names.push_back(name);
        }

        return names;
    }

    /**
     * @brief Get names of operations matching a type
     * @tparam OpClass Operation class type to match
     * @return Vector of matching operation names
     */
    template <typename OpClass>
    std::vector<std::string> list_names_by_type() const
    {
        std::shared_lock lock(m_mutex);

        std::type_index target_type(typeid(OpClass));
        std::vector<std::string> names;

        for (const auto& [name, info] : m_info) {
            if (info.type == target_type) {
                names.push_back(name);
            }
        }

        return names;
    }

    /**
     * @brief Get names matching a predicate
     * @param predicate Function to test each name
     * @return Vector of matching names
     */
    std::vector<std::string> find_names(const NamePredicate& predicate) const
    {
        std::shared_lock lock(m_mutex);

        std::vector<std::string> names;

        for (const auto& [name, _] : m_operations) {
            if (predicate(name)) {
                names.push_back(name);
            }
        }

        return names;
    }

    /**
     * @brief Check if operation exists
     * @param name Name to check
     * @return true if exists
     */
    bool has(const std::string& name) const
    {
        std::shared_lock lock(m_mutex);
        return m_operations.contains(name);
    }

    /**
     * @brief Check if any operations of a type exist
     * @tparam OpClass Operation class type to check
     * @return true if at least one exists
     */
    template <typename OpClass>
    bool has_type() const
    {
        std::shared_lock lock(m_mutex);

        std::type_index target_type(typeid(OpClass));

        return std::any_of(m_info.begin(), m_info.end(),
            [target_type](const auto& pair) {
                return pair.second.type == target_type;
            });
    }

    /**
     * @brief Get type of named operation
     * @param name Operation name
     * @return Optional containing the type index if found
     */
    std::optional<std::type_index> get_type(const std::string& name) const
    {
        std::shared_lock lock(m_mutex);

        auto it = m_info.find(name);
        if (it != m_info.end()) {
            return it->second.type;
        }

        return std::nullopt;
    }

    /**
     * @brief Get metadata about an operation
     * @param name Operation name
     * @return Optional containing the operation info if found
     */
    std::optional<PooledOperationInfo> get_info(const std::string& name) const
    {
        std::shared_lock lock(m_mutex);

        auto it = m_info.find(name);
        if (it != m_info.end()) {
            return it->second;
        }

        return std::nullopt;
    }

    /**
     * @brief Get pool size
     * @return Number of operations in the pool
     */
    size_t size() const
    {
        std::shared_lock lock(m_mutex);
        return m_operations.size();
    }

    /**
     * @brief Check if pool is empty
     * @return true if no operations in pool
     */
    bool empty() const
    {
        std::shared_lock lock(m_mutex);
        return m_operations.empty();
    }

    /**
     * @brief Get statistics about pool usage
     * @return Map of statistics
     */
    std::unordered_map<std::string, std::any> get_statistics() const
    {
        std::shared_lock lock(m_mutex);

        std::unordered_map<std::string, std::any> stats;
        stats["total_operations"] = m_operations.size();

        size_t total_accesses = 0;
        std::string most_accessed;
        size_t max_accesses = 0;

        for (const auto& [name, info] : m_info) {
            total_accesses += info.access_count;
            if (info.access_count > max_accesses) {
                max_accesses = info.access_count;
                most_accessed = name;
            }
        }

        stats["total_accesses"] = total_accesses;
        if (!most_accessed.empty()) {
            stats["most_accessed_operation"] = most_accessed;
            stats["most_accessed_count"] = max_accesses;
        }

        return stats;
    }

    /**
     * @brief Set callback for when operations are added
     */
    void on_add(std::function<void(const std::string&, std::type_index)> callback)
    {
        std::unique_lock lock(m_mutex);
        m_on_add_callback = std::move(callback);
    }

    /**
     * @brief Set callback for when operations are removed
     */
    void on_remove(std::function<void(const std::string&, std::type_index)> callback)
    {
        std::unique_lock lock(m_mutex);
        m_on_remove_callback = std::move(callback);
    }

    /**
     * @brief Set callback for when operations are replaced
     */
    void on_replace(std::function<void(const std::string&, std::type_index)> callback)
    {
        std::unique_lock lock(m_mutex);
        m_on_replace_callback = std::move(callback);
    }

    /**
     * @brief Add multiple operations at once
     * @tparam OpClass Operation class type
     * @param operations Map of names to operations
     * @return Number of operations successfully added
     */
    template <typename OpClass>
    size_t add_batch(const std::unordered_map<std::string, std::shared_ptr<OpClass>>& operations)
    {
        std::unique_lock lock(m_mutex);

        size_t added = 0;
        for (const auto& [name, op] : operations) {
            if (op && !m_operations.contains(name)) {
                m_operations[name] = std::static_pointer_cast<void>(op);

                PooledOperationInfo info;
                info.name = name;
                info.type = std::type_index(typeid(OpClass));
                info.created_at = std::chrono::steady_clock::now();
                info.last_accessed = info.created_at;
                info.access_count = 0;

                m_info[name] = std::move(info);
                added++;

                if (m_on_add_callback) {
                    m_on_add_callback(name, info.type);
                }
            }
        }

        return added;
    }

    /**
     * @brief Remove operations matching a predicate
     * @param predicate Function to test each name
     * @return Number of operations removed
     */
    size_t remove_if(const NamePredicate& predicate)
    {
        std::unique_lock lock(m_mutex);

        std::vector<std::string> to_remove;

        for (const auto& [name, _] : m_operations) {
            if (predicate(name)) {
                to_remove.push_back(name);
            }
        }

        for (const auto& name : to_remove) {
            auto type = m_info[name].type;
            m_operations.erase(name);
            m_info.erase(name);

            if (m_on_remove_callback) {
                m_on_remove_callback(name, type);
            }
        }

        return to_remove.size();
    }

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, OperationPtr> m_operations;
    std::unordered_map<std::string, PooledOperationInfo> m_info;

    std::function<void(const std::string&, std::type_index)> m_on_add_callback;
    std::function<void(const std::string&, std::type_index)> m_on_remove_callback;
    std::function<void(const std::string&, std::type_index)> m_on_replace_callback;
};

} // namespace MayaFlux::Yantra
