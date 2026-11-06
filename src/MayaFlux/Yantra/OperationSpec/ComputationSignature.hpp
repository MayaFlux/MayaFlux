#pragma once

namespace MayaFlux::Yantra {

/** @brief Ordered operation selections, preserving each enum's type and value. */
class ComputationSignature {
public:
    ComputationSignature() = default;

    template <typename E>
        requires std::is_enum_v<E>
    ComputationSignature(E selection)
    {
        static_assert(sizeof(std::underlying_type_t<E>) <= sizeof(uint64_t));
        m_selections.push_back({ enum_identity<E>(), static_cast<uint64_t>(selection) });
    }

    [[nodiscard]] bool empty() const { return m_selections.empty(); }
    [[nodiscard]] size_t size() const { return m_selections.size(); }

    /** @brief The single selection at this stage, retaining its enum identity. */
    [[nodiscard]] ComputationSignature at(size_t index) const
    {
        ComputationSignature result;
        result.m_selections.push_back(m_selections.at(index));
        return result;
    }

    /** @brief Read a selection when its enum type matches E. */
    template <typename E>
        requires std::is_enum_v<E>
    [[nodiscard]] std::optional<E> selection(size_t index) const
    {
        if (index >= size() || m_selections[index].type != enum_identity<E>()) {
            return std::nullopt;
        }
        return static_cast<E>(m_selections[index].value);
    }

    bool operator==(const ComputationSignature&) const = default;

    /** @brief Whether the signature begins with all selections in prefix. */
    [[nodiscard]] bool starts_with(const ComputationSignature& prefix) const
    {
        return prefix.size() <= size()
            && std::equal(prefix.m_selections.begin(), prefix.m_selections.end(), m_selections.begin());
    }

    /** @brief Remaining selections after a resolved prefix. */
    [[nodiscard]] ComputationSignature after(size_t count) const
    {
        ComputationSignature result;
        const auto offset = std::min(count, size());
        result.m_selections.assign(m_selections.begin() + offset, m_selections.end());
        return result;
    }

    friend ComputationSignature operator|(ComputationSignature left, const ComputationSignature& right)
    {
        left.m_selections.insert(left.m_selections.end(), right.m_selections.begin(), right.m_selections.end());
        return left;
    }

private:
    struct Selection {
        const void* type;
        uint64_t value;

        bool operator==(const Selection&) const = default;
    };

    template <typename E>
    static const void* enum_identity()
    {
        static const char identity {};
        return &identity;
    }

    std::vector<Selection> m_selections;
};

/** @brief Compose enum selections in execution order without combining their bits. */
template <typename Left, typename Right>
    requires(std::is_enum_v<Left> && std::is_enum_v<Right>)
ComputationSignature operator|(Left left, Right right)
{
    return ComputationSignature(left) | ComputationSignature(right);
}

}
