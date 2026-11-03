#pragma once

#include "MayaFlux/Transitive/Memory/FlatMap.hpp"

namespace MayaFlux {

namespace detail {
    struct FacetToken {
    };
}

/**
 * @class Facet
 * @brief Typed key for an arbitrary attribute stored in a Facets bag.
 *
 * The type is fixed by the key. A key carries an initial value, so Facet(0.5F)
 * deduces Facet<float> and Facet<double>() value-initialises one. Copies share
 * the identity, so a copied key addresses the same attribute.
 *
 * @tparam T The attribute type. Must be an object type that is not const.
 */
template <typename T>
class Facet {
    static_assert(std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T>,
        "Facet<T> requires a non-const, non-volatile object type");

public:
    explicit Facet(T initial)
        : m_token(std::make_shared<detail::FacetToken>())
        , m_initial(std::move(initial))
    {
    }

    Facet() requires std::default_initializable<T>
        : m_token(std::make_shared<detail::FacetToken>())
        , m_initial()
    {
    }

    /** @brief Identity shared by every copy of this key. */
    [[nodiscard]] const void* id() const noexcept { return m_token.get(); }

    /** @brief Keeps the identity alive while a Facets bag holds a value for it. */
    [[nodiscard]] const std::shared_ptr<const void>& token() const noexcept { return m_token; }

    /** @brief Value stored by set(key) with no explicit value. */
    [[nodiscard]] const T& initial() const noexcept { return m_initial; }

private:
    std::shared_ptr<const void> m_token;
    T m_initial;
};

namespace detail {
    template <auto Member>
    struct MemberOf;

    template <typename Owner, typename T, T Owner::*Member>
    struct MemberOf<Member> {
        using value_type = T;
    };

    template <auto Member>
    const void* member_identity() noexcept
    {
        static constexpr char tag = 0;
        return &tag;
    }
}

/**
 * @class FacetMember
 * @brief Key for one data member of a struct, identified by its member pointer.
 *
 * A struct declares its keys with a static describe_facets() returning a tuple
 * of FacetMember values. Every instance of the struct produces the same keys.
 *
 * @tparam Member Pointer to a non-static data member.
 */
template <auto Member>
struct FacetMember {
    using value_type = typename detail::MemberOf<Member>::value_type;

    static constexpr auto member = Member;

    /** @brief Identity shared by every instance of the owning struct. */
    [[nodiscard]] static const void* id() noexcept { return detail::member_identity<Member>(); }
};

/**
 * @class Facets
 * @brief Arbitrary typed attributes keyed by Facet objects.
 *
 * Values are held as shared immutable entries, so copying a Facets copies
 * pointers and never the values. Writing replaces an entry; get() returns a
 * const view that stays valid until that entry is replaced or erased.
 */
class Facets {
public:
    /** @brief Stores @p value under @p key, replacing any previous value. */
    template <typename T>
    void set(const Facet<T>& key, std::type_identity_t<T> value)
    {
        m_entries.insert_or_assign(key.id(), std::make_shared<Entry<T>>(key.token(), std::move(value)));
    }

    /** @brief Stores the key's initial value under @p key. */
    template <typename T>
    void set(const Facet<T>& key)
    {
        set(key, key.initial());
    }

    /** @brief Stores @p value under the member key @p key, replacing any previous value. */
    template <auto Member>
    void set(FacetMember<Member>, std::type_identity_t<typename FacetMember<Member>::value_type> value)
    {
        using T = typename FacetMember<Member>::value_type;
        m_entries.insert_or_assign(FacetMember<Member>::id(), std::make_shared<Entry<T>>(std::shared_ptr<const void> {}, std::move(value)));
    }

    /**
     * @brief Builds a bag holding every data member of @p value.
     * @tparam S A struct with a static describe_facets() returning a tuple of FacetMember.
     * @return One entry per member, keyed by the member, with its current value.
     */
    template <typename S>
    requires requires { S::describe_facets(); }
    [[nodiscard]] static Facets from(const S& value)
    {
        Facets bag;
        std::apply(
            [&](auto... keys) {
                (bag.set(keys, value.*(decltype(keys)::member)), ...);
            },
            S::describe_facets());
        return bag;
    }

    /** @brief The value under @p key, or nullptr when absent. */
    template <typename T>
    [[nodiscard]] const T* get(const Facet<T>& key) const
    {
        const auto it = m_entries.find(key.id());
        if (it == m_entries.end()) {
            return nullptr;
        }
        return &static_cast<const Entry<T>*>(it->second.get())->value;
    }

    /** @brief The value under the member key @p key, or nullptr when absent. */
    template <auto Member>
    [[nodiscard]] const typename FacetMember<Member>::value_type* get(FacetMember<Member>) const
    {
        using T = typename FacetMember<Member>::value_type;
        const auto it = m_entries.find(FacetMember<Member>::id());
        if (it == m_entries.end()) {
            return nullptr;
        }
        return &static_cast<const Entry<T>*>(it->second.get())->value;
    }

    /** @brief True when @p key has a value. */
    template <typename T>
    [[nodiscard]] bool has(const Facet<T>& key) const
    {
        return m_entries.find(key.id()) != m_entries.end();
    }

    /** @brief Removes the value under @p key. @return true if there was one. */
    template <typename T>
    bool erase(const Facet<T>& key)
    {
        return m_entries.erase(key.id());
    }

    [[nodiscard]] size_t size() const noexcept { return m_entries.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_entries.empty(); }
    void clear() noexcept { m_entries.clear(); }

    /** @brief Adds entries from @p source, replacing values for shared keys. */
    void merge(const Facets& source)
    {
        for (const auto& [id, entry] : source.m_entries) {
            m_entries.insert_or_assign(id, entry);
        }
    }

private:
    struct EntryBase {
        std::shared_ptr<const void> token;

        explicit EntryBase(std::shared_ptr<const void> t)
            : token(std::move(t))
        {
        }

        virtual ~EntryBase() = default;
    };

    template <typename T>
    struct Entry final : EntryBase {
        Entry(std::shared_ptr<const void> t, T v)
            : EntryBase(std::move(t))
            , value(std::move(v))
        {
        }

        T value;
    };

    Memory::FlatMap<const void*, std::shared_ptr<const EntryBase>> m_entries;
};

} // namespace MayaFlux
