#pragma once

namespace Lila {

/**
 * @brief Tracks symbols declared in live coding sessions
 *
 * Simple registry that remembers what was declared so we can
 * emit extern declarations in subsequent evals.
 */
class SymbolRegistry {
public:
    struct Symbol {
        std::string name;
        std::string type;
        std::string namespace_name; // e.g., "__live_v0"
        size_t version;
    };

    /**
     * @brief Register a new symbol from current eval
     */
    void add_symbol(const std::string& name,
        const std::string& type,
        const std::string& namespace_name,
        size_t version)
    {
        Symbol sym { .name = name, .type = type, .namespace_name = namespace_name, .version = version };
        m_symbols[name] = sym;
    }

    /**
     * @brief Get all known symbols
     */
    const std::unordered_map<std::string, Symbol>& get_symbols() const
    {
        return m_symbols;
    }

    /**
     * @brief Check if symbol exists
     */
    bool has_symbol(const std::string& name) const
    {
        return m_symbols.find(name) != m_symbols.end();
    }

    /**
     * @brief Get specific symbol info
     */
    const Symbol* get_symbol(const std::string& name) const
    {
        auto it = m_symbols.find(name);
        return (it != m_symbols.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Clear all symbols (for reset)
     */
    void clear()
    {
        m_symbols.clear();
    }

private:
    std::unordered_map<std::string, Symbol> m_symbols;
};

} // namespace Lila
