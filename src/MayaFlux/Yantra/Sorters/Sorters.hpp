#pragma once

#include "MayaFlux/Yantra/ComputeMatrix.hpp"

namespace MayaFlux::Kakshya {
struct RegionSegment;
class SignalSourceContainer;
}

namespace MayaFlux::Yantra {

/**
 * @class DataSorter
 * @brief Base template class for operations that reorder collections of data
 *
 * Defines the interface for all sorting operations that can be applied to
 * collections of items. Supports customizable comparison logic through
 * function objects.
 *
 * @tparam ItemType The type of items to be sorted
 */
template <typename ItemType>
class DataSorter : public ComputeOperation<std::vector<ItemType>, std::vector<ItemType>> {
public:
    /**
     * @brief Sorts the input collection according to the sorter's algorithm
     * @param input Vector of items to be sorted
     * @return Sorted vector of items
     */
    virtual std::vector<ItemType> apply_operation(std::vector<ItemType> input) override = 0;

    /**
     * @brief Sets a custom comparison function for determining item order
     * @param comp Function that returns true if first argument should precede second
     */
    virtual void set_comparison_function(std::function<bool(const ItemType&, const ItemType&)> comp)
    {
        m_comparison_function = comp;
    }

protected:
    /** @brief Function object defining the ordering relation between items */
    std::function<bool(const ItemType&, const ItemType&)> m_comparison_function;
};

/**
 * @class RegionDurationSorter
 * @brief Sorter that orders region segments based on their temporal duration
 *
 * Arranges region segments in ascending or descending order according to
 * the length of time they span.
 */
class RegionDurationSorter : public DataSorter<Kakshya::RegionSegment> {
public:
    /**
     * @brief Constructs a duration-based region sorter
     * @param ascending If true, sorts from shortest to longest; if false, longest to shortest
     */
    RegionDurationSorter(bool ascending = true);

    /**
     * @brief Sorts region segments by their duration
     * @param input Vector of region segments to sort
     * @return Vector of region segments sorted by duration
     */
    virtual std::vector<Kakshya::RegionSegment> apply_operation(
        std::vector<Kakshya::RegionSegment> input) override;

private:
    /** @brief Direction of sort (true = ascending, false = descending) */
    bool m_ascending;
};

/**
 * @class RegionEnergySorter
 * @brief Sorter that orders region segments based on their signal energy
 *
 * Arranges region segments in ascending or descending order according to
 * the energy content of the signal within each segment.
 */
class RegionEnergySorter : public DataSorter<Kakshya::RegionSegment> {
public:
    /**
     * @brief Constructs an energy-based region sorter
     * @param container Signal source containing the data for energy calculation
     * @param ascending If true, sorts from lowest to highest energy; if false, highest to lowest
     */
    RegionEnergySorter(std::shared_ptr<Kakshya::SignalSourceContainer> container, bool ascending = true);

    /**
     * @brief Sorts region segments by their signal energy
     * @param input Vector of region segments to sort
     * @return Vector of region segments sorted by energy content
     */
    virtual std::vector<Kakshya::RegionSegment> apply_operation(
        std::vector<Kakshya::RegionSegment> input) override;

private:
    /** @brief Signal container used for energy calculations */
    std::shared_ptr<Kakshya::SignalSourceContainer> m_container;
    /** @brief Direction of sort (true = ascending, false = descending) */
    bool m_ascending;
};

/**
 * @class ValueSorter
 * @brief Generic sorter for collections of comparable values
 *
 * Arranges items in ascending or descending order using their natural
 * comparison operators or a custom comparison function.
 *
 * @tparam T The type of values to be sorted
 */
template <typename T>
class ValueSorter : public DataSorter<T> {
public:
    /**
     * @brief Constructs a generic value sorter
     * @param ascending If true, sorts in ascending order; if false, descending order
     */
    ValueSorter(bool ascending = true);

    /**
     * @brief Sorts values according to their comparison relation
     * @param input Vector of values to sort
     * @return Sorted vector of values
     */
    virtual std::vector<T> apply_operation(std::vector<T> input) override;

private:
    /** @brief Direction of sort (true = ascending, false = descending) */
    bool m_ascending;
};
}
