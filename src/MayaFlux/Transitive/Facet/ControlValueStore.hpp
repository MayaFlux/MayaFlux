#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

namespace MayaFlux {

/**
 * @concept ControlSlot
 * @brief What ControlValueStore needs of the slot type it owns.
 *
 * A slot reports has_value, takes another slot's state with assign, can be
 * settled and refreshed from a source. ControlValue, ControlSnapshot and
 * anything derived from them qualify.
 */
template <typename S>
concept ControlSlot = requires(S slot, const S& other) {
    { slot.has_value() } -> std::convertible_to<bool>;
    slot.assign(other);
    slot.refresh();
    slot.settle();
};

/**
 * @concept NumericControlSlot
 * @brief A ControlSlot that also reads as a number, which the named access and the scale mirror need.
 *
 * It reports width, reads as a double, hands out a pending value once per
 * change, is shown the owner's real value with observe, and is made from a
 * number. ControlValue and anything derived from it qualify.
 */
template <typename S>
concept NumericControlSlot = ControlSlot<S> && requires(S slot, double number) {
    { slot.width() } -> std::convertible_to<size_t>;
    { slot.value() } -> std::convertible_to<double>;
    { slot.pending() } -> std::same_as<std::optional<double>>;
    slot.observe(number);
    S(number);
};

/**
 * @class ControlValueStore
 * @brief Owns one described struct of Slot members for a host such as a network, an operator or an input node.
 *
 * A host holds one as a member and forwards to it. It is not a base class.
 * declare() creates the struct and keeps it for the host's lifetime, merge()
 * applies the members an incoming struct holds, snapshot() copies the struct for
 * get(), and refresh() reads sources once per block. A member named `scale` is
 * mirrored to a value the host passes in, and a host with no scale passes nothing.
 * Every member is a Slot or a described struct of the same, nested to any depth.
 *
 * A member is also reachable by name through set_named() and read_named(), which
 * back an operator's string based set_parameter and query_state. A member of a
 * nested struct is named by its path joined with dots, such as `left.deadzone`.
 * Only a top level member named `scale` is mirrored. Named number access and the
 * scale mirror need a NumericControlSlot.
 *
 * @tparam Slot A ControlSlot, the one slot type of every member.
 */
template <ControlSlot Slot>
class ControlValueStore {
public:
    ControlValueStore() = default;
    ~ControlValueStore() = default;

    ControlValueStore(const ControlValueStore&) = delete;
    ControlValueStore& operator=(const ControlValueStore&) = delete;
    ControlValueStore(ControlValueStore&&) = delete;
    ControlValueStore& operator=(ControlValueStore&&) = delete;

    /** @brief Creates the struct and returns it, for the host to hold by reference. */
    template <typename C>
    [[nodiscard]] C& declare()
    {
        auto storage = std::make_shared<C>();
        C& controls = *storage;

        m_storage = std::move(storage);
        m_tag = tag<C>();
        m_slots.clear();
        m_scale = nullptr;

        add_members(controls, std::string());

        return controls;
    }

    /** @brief Applies every member of @p incoming that holds a value. */
    template <typename C>
    void merge(const C& incoming)
    {
        if (m_tag != tag<C>()) {
            return;
        }

        merge_members(*static_cast<C*>(m_storage.get()), incoming);
    }

    /**
     * @brief A copy of the struct.
     * @param scale The value a top level member named `scale` really holds, or null.
     */
    template <typename C>
    [[nodiscard]] C snapshot(const double* scale = nullptr) const
    {
        C current = m_tag == tag<C>()
            ? *static_cast<const C*>(m_storage.get())
            : C {};
        if (scale) {
            std::apply(
                [&](const auto&... member) {
                    (observe_scale(member, current, *scale), ...);
                },
                Reflect::Generated::Description<C>::members());
        }
        return current;
    }

    /**
     * @brief Reads sources into their slots.
     * @param scale Where a member named `scale` is written when it is due, or null.
     */
    void refresh(double* scale = nullptr)
    {
        for (const Entry& entry : m_slots) {
            entry.slot->refresh();
        }

        if constexpr (NumericControlSlot<Slot>) {
            if (scale && m_scale) {
                if (const auto due = m_scale->pending()) {
                    *scale = std::max(0.0, *due);
                }
            }
        }
    }

    /**
     * @brief Sets the member called @p name to the number @p value.
     * @return False when no member has that name.
     *
     * The same as set() with that one member, so it applies at the next block.
     * A member that holds a vector is left alone, so a number cannot retype it.
     */
    bool set_named(std::string_view name, double value)
        requires NumericControlSlot<Slot>
    {
        for (const Entry& entry : m_slots) {
            if (entry.name == name) {
                if (entry.slot->width() > 1) {
                    return false;
                }
                entry.slot->assign(Slot(value));
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Drops the pending value of the member called @p name.
     * @return False when no member has that name.
     *
     * For an immediate write made by name: a typed value set earlier would
     * otherwise be applied at the next block over it.
     */
    bool settle_named(std::string_view name)
    {
        for (const Entry& entry : m_slots) {
            if (entry.name == name) {
                entry.slot->settle();
                return true;
            }
        }
        return false;
    }

    /**
     * @brief The number held by the member called @p name, or nullopt for none or a vector.
     * @param controls A copy from get(), so values the owner holds outside the struct are real.
     */
    template <typename C>
        requires NumericControlSlot<Slot>
    [[nodiscard]] static std::optional<double> read_named(const C& controls, std::string_view name)
    {
        std::optional<double> found;
        std::apply(
            [&](const auto&... member) {
                (read_member(member, controls, name, found), ...);
            },
            Reflect::Generated::Description<C>::members());
        return found;
    }

private:
    struct Entry {
        std::string name;
        Slot* slot {};
    };

    template <typename C>
    [[nodiscard]] static const void* tag() noexcept
    {
        static constexpr char value = 0;
        return &value;
    }

    template <typename C>
    void add_members(C& owner, const std::string& prefix)
    {
        std::apply(
            [&](const auto&... member) {
                (add_member(member, owner, prefix), ...);
            },
            Reflect::Generated::Description<C>::members());
    }

    template <typename Member, typename C>
    void add_member(const Member& member, C& owner, const std::string& prefix)
    {
        using Value = typename Member::value_type;
        auto& value = member.get(owner);
        if constexpr (std::same_as<Value, Slot>) {
            m_slots.push_back({ prefix + std::string(member.name), &value });
            if constexpr (NumericControlSlot<Slot>) {
                if (prefix.empty() && member.name == "scale") {
                    m_scale = &value;
                }
            }
        } else {
            static_assert(Reflect::Described<Value>,
                "every member of a controls struct must be the store's slot type or a described struct of them");
            add_members(value, prefix + std::string(member.name) + ".");
        }
    }

    template <typename C>
    static void merge_members(C& live, const C& incoming)
    {
        std::apply(
            [&](const auto&... member) {
                (merge_member(member, live, incoming), ...);
            },
            Reflect::Generated::Description<C>::members());
    }

    template <typename Member, typename C>
    static void merge_member(const Member& member, C& live, const C& incoming)
    {
        using Value = typename Member::value_type;
        if constexpr (std::same_as<Value, Slot>) {
            const Slot& given = member.get(incoming);
            if (given.has_value()) {
                member.get(live).assign(given);
            }
        } else {
            merge_members(member.get(live), member.get(incoming));
        }
    }

    template <typename Member, typename C>
    static void observe_scale(const Member& member, C& current, double scale)
    {
        if constexpr (NumericControlSlot<Slot> && std::same_as<typename Member::value_type, Slot>) {
            if (member.name == "scale") {
                member.get(current).observe(scale);
            }
        }
    }

    template <typename Member, typename C>
    static void read_member(const Member& member, const C& owner, std::string_view name,
        std::optional<double>& found)
    {
        const auto& value = member.get(owner);
        if constexpr (std::same_as<typename Member::value_type, Slot>) {
            if (member.name == name && value.has_value() && value.width() == 1) {
                found = value.value();
            }
        } else if (name.size() > member.name.size() && name.starts_with(member.name)
            && name[member.name.size()] == '.') {
            if (const auto inner = read_named(value, name.substr(member.name.size() + 1))) {
                found = inner;
            }
        }
    }

    std::shared_ptr<void> m_storage;
    const void* m_tag = nullptr;
    std::vector<Entry> m_slots;
    Slot* m_scale = nullptr;
};

/**
 * @class ControlValueTable
 * @brief One ControlValueStore per key, for a host with a runtime count of tunable groups.
 *
 * Each key owns its own store and so its own described struct, for example one
 * per control of a device. declare() creates or replaces the store of a key, and
 * the store is then used through find(). Adding or removing a key belongs
 * outside the block that reads the stores, as with any source change.
 *
 * @tparam Key An ordered key type.
 * @tparam Slot A ControlSlot, the slot type of every store.
 */
template <typename Key, ControlSlot Slot>
class ControlValueTable {
public:
    using Store = ControlValueStore<Slot>;

    /** @brief Creates the store of @p key, replacing any it had, and returns its struct. */
    template <typename C>
    [[nodiscard]] C& declare(const Key& key)
    {
        auto& store = m_stores[key];
        store = std::make_unique<Store>();
        return store->template declare<C>();
    }

    /** @brief The store of @p key, or null when it has none. */
    [[nodiscard]] Store* find(const Key& key) noexcept
    {
        const auto found = m_stores.find(key);
        return found == m_stores.end() ? nullptr : found->second.get();
    }

    [[nodiscard]] const Store* find(const Key& key) const noexcept
    {
        const auto found = m_stores.find(key);
        return found == m_stores.end() ? nullptr : found->second.get();
    }

    /** @brief Drops the store of @p key. @return False when it had none. */
    bool erase(const Key& key) { return m_stores.erase(key) != 0; }

    [[nodiscard]] size_t size() const noexcept { return m_stores.size(); }

    /** @brief Reads the sources of every store. */
    void refresh()
    {
        for (const auto& entry : m_stores) {
            entry.second->refresh();
        }
    }

    /** @brief Calls visit(key, store) for every store in key order. */
    template <typename Visit>
    void for_each(const Visit& visit)
    {
        for (const auto& entry : m_stores) {
            visit(entry.first, *entry.second);
        }
    }

private:
    std::map<Key, std::unique_ptr<Store>> m_stores;
};

}
