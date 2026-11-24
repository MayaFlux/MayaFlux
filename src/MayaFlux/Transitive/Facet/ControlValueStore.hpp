#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"

#include <typeindex>

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
 * @class ControlHeld
 * @brief A frozen copy of the structs a ControlValueStore holds, read back by type.
 *
 * For a reader that must see one set of values for a whole piece of work, such as
 * a rewrite pass or a frame, and that cannot name the struct types when it is
 * given the copy. Cheap to pass around: it shares its copies.
 */
class ControlHeld {
public:
    struct Part {
        std::type_index tag { typeid(void) };
        std::shared_ptr<const void> data;
    };

    ControlHeld() = default;

    explicit ControlHeld(std::vector<Part> parts)
        : m_parts(std::move(parts))
    {
    }

    /**
     * @brief The held struct of type C.
     * @return A default one when no struct of that type was held.
     */
    template <typename C>
    [[nodiscard]] const C& get() const noexcept
    {
        static const C none {};
        for (const Part& part : m_parts) {
            if (part.tag == std::type_index(typeid(C)) && part.data) {
                return *static_cast<const C*>(part.data.get());
            }
        }
        return none;
    }

    [[nodiscard]] bool empty() const noexcept { return m_parts.empty(); }

private:
    std::vector<Part> m_parts;
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
 * Every member is a Slot, another ControlSlot type, or a described struct of
 * these, nested to any depth. A member of another slot type is merged, copied,
 * refreshed and settled like the others, but has no named number access.
 *
 * extend() adds a second described struct beside the first, such as one a user
 * declares, with its members named by the struct's own name and a dot. declare()
 * starts over and drops every extension. Like declare(), extend() belongs before
 * any other thread uses the store.
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

        m_slots.clear();
        m_others.clear();
        m_extensions.clear();
        m_storage = std::move(storage);
        m_tag = tag<C>();
        m_snapshot = &snapshot_of<C>;
        m_scale = nullptr;

        add_members(controls, std::string());

        return controls;
    }

    /**
     * @brief Adds a second struct beside the declared one and returns it.
     *
     * Its members are named by the last part of its described name, a dot and the
     * member, so `Bloom.rate`, and never take the `scale` mirror. A struct that is
     * already held, the declared one included, is returned as it is.
     */
    template <typename C>
    [[nodiscard]] C& extend()
    {
        if (C* held = locate<C>()) {
            return *held;
        }

        auto storage = std::make_shared<C>();
        C& controls = *storage;
        m_extensions.push_back({ tag<C>(), std::move(storage), &snapshot_of<C> });

        add_members(controls, std::string(short_name(Reflect::Generated::Description<C>::name)) + ".");

        return controls;
    }

    /**
     * @brief The held struct of type C, or null when none is.
     *
     * The live struct and not a copy, so a member can be read without copying the
     * rest. Its slots are safe to read from any thread.
     */
    template <typename C>
    [[nodiscard]] const C* live() const noexcept
    {
        return locate<C>();
    }

    /**
     * @brief A frozen copy of every struct the store holds, as they are now.
     *
     * The scale mirror is not applied, so a host that has a scale reads its own.
     */
    [[nodiscard]] ControlHeld held() const
    {
        std::vector<ControlHeld::Part> parts;
        if (m_storage && m_snapshot) {
            parts.push_back({ .tag = m_tag, .data = m_snapshot(m_storage.get()) });
        }
        for (const Extension& extension : m_extensions) {
            parts.push_back({ .tag = extension.tag, .data = extension.snapshot(extension.storage.get()) });
        }
        return ControlHeld(std::move(parts));
    }

    /**
     * @brief The number the member called @p name holds now, read without copying any struct.
     * @return nullopt for an unknown name, an empty member or a member that holds a vector.
     *
     * A member of an extension is named as set_named names it.
     */
    [[nodiscard]] std::optional<double> number(std::string_view name) const
        requires NumericControlSlot<Slot>
    {
        const Slot* slot = find(name);
        if (slot && slot->has_value() && slot->width() == 1) {
            return slot->value();
        }
        return std::nullopt;
    }

    /** @brief The slot called @p name, or null when no member has that name. */
    [[nodiscard]] Slot* find(std::string_view name) const noexcept
    {
        for (const Entry& entry : m_slots) {
            if (entry.name == name) {
                return entry.slot;
            }
        }
        return nullptr;
    }

    /** @brief Calls visit(name, slot) for every member of the held structs, in declaration order. */
    template <typename Visit>
    void for_each(const Visit& visit) const
    {
        for (const Entry& entry : m_slots) {
            visit(std::string_view(entry.name), *entry.slot);
        }
    }

    /** @brief Applies every member of @p incoming that holds a value or asks for a clear. */
    template <typename C>
    void merge(const C& incoming)
    {
        if (C* held = locate<C>()) {
            merge_members(*held, incoming);
        }
    }

    /**
     * @brief A copy of the struct.
     * @param scale The value a top level member named `scale` really holds, or null.
     */
    template <typename C>
    [[nodiscard]] C snapshot(const double* scale = nullptr) const
    {
        const C* held = locate<C>();
        C current = held ? *held : C {};
        if (scale && m_tag == tag<C>()) {
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
        for (const Other& other : m_others) {
            other.refresh(other.slot);
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
     * @brief Gives the member called @p name the whole of @p value, a vector, a box or a source included.
     * @return False when no member has that name, or when the member already holds a value of another width.
     *
     * The same as set() with that one member. A member that is empty takes any width.
     */
    bool set_named(std::string_view name, const Slot& value)
        requires NumericControlSlot<Slot>
    {
        for (const Entry& entry : m_slots) {
            if (entry.name == name) {
                if (entry.slot->has_value() && value.has_value() && entry.slot->width() != value.width()) {
                    return false;
                }
                entry.slot->assign(value);
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
        for (const Other& other : m_others) {
            if (other.name == name) {
                other.settle(other.slot);
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

    struct Other {
        std::string name;
        void* slot {};
        void (*refresh)(void*) {};
        void (*settle)(void*) {};
    };

    using SnapshotFn = std::shared_ptr<const void> (*)(const void*);

    struct Extension {
        std::type_index tag { typeid(void) };
        std::shared_ptr<void> storage;
        SnapshotFn snapshot {};
    };

    template <typename C>
    [[nodiscard]] static std::type_index tag() noexcept
    {
        return std::type_index(typeid(C));
    }

    template <typename C>
    [[nodiscard]] static std::shared_ptr<const void> snapshot_of(const void* held)
    {
        return std::make_shared<const C>(*static_cast<const C*>(held));
    }

    [[nodiscard]] static std::string_view short_name(std::string_view qualified) noexcept
    {
        const size_t cut = qualified.rfind("::");
        return cut == std::string_view::npos ? qualified : qualified.substr(cut + 2);
    }

    template <typename C>
    [[nodiscard]] C* locate() const noexcept
    {
        if (m_tag == tag<C>()) {
            return static_cast<C*>(m_storage.get());
        }
        for (const Extension& extension : m_extensions) {
            if (extension.tag == tag<C>()) {
                return static_cast<C*>(extension.storage.get());
            }
        }
        return nullptr;
    }

    template <typename S>
    [[nodiscard]] static bool requests_clear(const S& slot) noexcept
    {
        if constexpr (requires { { slot.clear_requested() } -> std::convertible_to<bool>; }) {
            return slot.clear_requested();
        } else {
            return false;
        }
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
        } else if constexpr (ControlSlot<Value>) {
            m_others.push_back({ prefix + std::string(member.name), &value,
                [](void* slot) { static_cast<Value*>(slot)->refresh(); },
                [](void* slot) { static_cast<Value*>(slot)->settle(); } });
        } else {
            static_assert(Reflect::Described<Value>,
                "every member of a controls struct must be a control slot or a described struct of them");
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
        if constexpr (ControlSlot<Value>) {
            const Value& given = member.get(incoming);
            if (given.has_value() || requests_clear(given)) {
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
        } else if constexpr (Reflect::Described<typename Member::value_type>) {
            if (name.size() > member.name.size() && name.starts_with(member.name)
                && name[member.name.size()] == '.') {
                if (const auto inner = read_named(value, name.substr(member.name.size() + 1))) {
                    found = inner;
                }
            }
        }
    }

    std::shared_ptr<void> m_storage;
    std::type_index m_tag { typeid(void) };
    std::vector<Entry> m_slots;
    std::vector<Other> m_others;
    std::vector<Extension> m_extensions;
    SnapshotFn m_snapshot = nullptr;
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
