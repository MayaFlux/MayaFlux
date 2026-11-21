#pragma once

#include "MayaFlux/Kakshya/NDimensionalContainer.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SemanticContainer
 * @brief The semantic half of a container over one NDData struct: its structure
 *        metadata and its named region groups.
 *
 * Specializes NDDataContainer<T> directly, so a derived container carries no
 * ProcessingState and no DataProcessor lifecycle. This class holds what every
 * such container keeps the same way: the structure metadata, a named region
 * group store, and the copy of a group's regions through get_region_data(). It
 * says nothing about where the data lives. The data, its dimensions, and
 * everything that reads or writes it stay in the derived class.
 *
 * @tparam T The NDData struct the container serves, such as TensorData.
 */
template <typename T>
class MAYAFLUX_API SemanticContainer : public NDDataContainer<T> {
public:
    /**
     * @brief Copy the regions of a group into separate values.
     * @return One value per region get_region_data() accepts; shorter than
     * group.regions when a region is invalid.
     */
    [[nodiscard]] std::vector<T> get_region_group_data(const RegionGroup& group) const override
    {
        std::vector<T> result;
        result.reserve(group.regions.size());

        for (const auto& region : group.regions) {
            auto copied = this->get_region_data(region);
            if (!copied.empty()) {
                result.push_back(std::move(copied.front()));
            }
        }
        return result;
    }

    /** @brief Store or replace a named region group. */
    void add_region_group(const RegionGroup& group) override
    {
        m_region_groups[group.name] = group;
    }

    /** @brief Return a named region group, or an empty group if absent. */
    [[nodiscard]] RegionGroup get_region_group(const std::string& name) const override
    {
        const auto it = m_region_groups.find(name);
        return it == m_region_groups.end() ? RegionGroup {} : it->second;
    }

    /** @brief Copy all named region groups. */
    [[nodiscard]] std::unordered_map<std::string, RegionGroup> get_all_region_groups() const override
    {
        return m_region_groups;
    }

    /** @brief Remove a named region group if present. */
    void remove_region_group(const std::string& name) override
    {
        m_region_groups.erase(name);
    }

    /** @brief Mutable structure metadata. */
    [[nodiscard]] ContainerDataStructure& get_structure() override { return m_structure; }

    /** @brief Structure metadata. */
    [[nodiscard]] const ContainerDataStructure& get_structure() const override { return m_structure; }

    /** @brief Replace structure metadata. */
    void set_structure(ContainerDataStructure structure) override { m_structure = std::move(structure); }

protected:
    /**
     * @brief Start with the structure metadata the derived container describes itself by.
     */
    explicit SemanticContainer(ContainerDataStructure structure)
        : m_structure(std::move(structure))
    {
    }

    /** @brief Drop every named region group. */
    void clear_region_groups() noexcept { m_region_groups.clear(); }

private:
    ContainerDataStructure m_structure;
    std::unordered_map<std::string, RegionGroup> m_region_groups;
};

}
