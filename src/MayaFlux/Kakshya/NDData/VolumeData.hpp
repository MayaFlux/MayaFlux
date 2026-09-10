#pragma once

#include "MayaFlux/Kinesis/Spatial/Lattice.hpp"
#include "MayaFlux/Kinesis/Spatial/LatticeSemantics.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct VolumeField
 * @brief One named quantity sampled over a lattice, held in host memory.
 *
 * Value storage is a variant over float and glm::vec3, chosen by the
 * producer from the source field's per-cell stride. There is no vec4
 * variant: padding is a GPU alignment concern and is stripped on the way
 * out, so no consumer of this struct ever sees a component it must skip.
 *
 * Values are stored densely in the lattice's own index order, x fastest,
 * z slowest, one element per cell with no gaps. Sparsity is a property of
 * the formats this feeds, not of this struct: a writer decides which cells
 * are active by thresholding, and the decision does not travel here.
 *
 * The name is the producer's own string, taken verbatim from wherever the
 * field was declared. Consumers that care about conventional names
 * (density, temperature, velocity) match on it; nothing rewrites it.
 */
struct VolumeField {
    using ValueStorage = std::variant<
        std::vector<float>,
        std::vector<glm::vec3>>;

    std::string name;
    Kinesis::LatticeSemantics semantics {};
    ValueStorage values;

    /**
     * @brief Whether the active variant holds vectors rather than scalars.
     */
    [[nodiscard]] bool is_vector() const
    {
        return std::holds_alternative<std::vector<glm::vec3>>(values);
    }

    /**
     * @brief Number of cells represented, dispatched on variant.
     *
     * One element per cell in both cases, so this is a cell count and not
     * a component count.
     */
    [[nodiscard]] size_t element_count() const
    {
        return std::visit(
            [](const auto& vec) { return vec.size(); },
            values);
    }

    /**
     * @brief Total byte size of value storage, dispatched on variant.
     */
    [[nodiscard]] size_t byte_size() const
    {
        return std::visit(
            [](const auto& vec) {
                return vec.size() * sizeof(typename std::decay_t<decltype(vec)>::value_type);
            },
            values);
    }

    /**
     * @brief Raw data pointer, dispatched on variant. For writer paths.
     */
    [[nodiscard]] const void* data() const
    {
        return std::visit(
            [](const auto& vec) -> const void* { return vec.data(); },
            values);
    }

    /**
     * @brief Typed accessors. Return nullptr if the variant does not match.
     */
    [[nodiscard]] const std::vector<float>* as_scalar() const { return std::get_if<std::vector<float>>(&values); }
    [[nodiscard]] const std::vector<glm::vec3>* as_vector() const { return std::get_if<std::vector<glm::vec3>>(&values); }

    [[nodiscard]] std::vector<float>* as_scalar() { return std::get_if<std::vector<float>>(&values); }
    [[nodiscard]] std::vector<glm::vec3>* as_vector() { return std::get_if<std::vector<glm::vec3>>(&values); }
};

/**
 * @struct VolumeData
 * @brief A lattice and every field sampled over it, held in host memory.
 *
 * The interchange currency between whatever produced the values and
 * whatever writes them. Parallels ImageData: no Vulkan awareness, no
 * ownership of GPU resources, no knowledge of any file format. A writer
 * receives one of these and nothing else.
 *
 * All fields share the lattice. A producer wanting fields at differing
 * resolutions emits several VolumeData rather than one, which matches how
 * every target format treats a resolution change anyway.
 *
 * The lattice carries the world-space bounds and resolution, so a writer
 * derives voxel size from cell_size() and the index-to-world mapping from
 * cell_center() without further arguments. There is no time member: a
 * frame sequence is a sequence of these, numbered by the caller.
 *
 * Field order is the producer's order and is preserved. Nothing depends on
 * it, but a reader diffing two files will thank you.
 */
struct VolumeData {
    Kinesis::Lattice3D lattice;
    std::vector<VolumeField> fields;

    /**
     * @brief Cells in the lattice, which every field's element_count()
     *        must equal.
     */
    [[nodiscard]] size_t cell_count() const { return lattice.cell_count(); }

    /**
     * @brief Resolve a field by name.
     * @param name Field name, matched exactly.
     * @return Pointer to the field, or nullptr if no field carries that name.
     */
    [[nodiscard]] const VolumeField* find(const std::string& name) const
    {
        for (const auto& field : fields) {
            if (field.name == name) {
                return &field;
            }
        }
        return nullptr;
    }

    /**
     * @brief Check that every field is densely populated over the lattice.
     *
     * Verifies a nonzero lattice, at least one field, no empty or duplicated
     * names, and that each field holds exactly cell_count() elements.
     *
     * Producers should invoke this before handing the data to a writer.
     * Writers should invoke it before trusting any pointer they take from
     * it, as EXRWriter does with ImageData.
     */
    [[nodiscard]] bool is_consistent() const
    {
        if (cell_count() == 0 || fields.empty()) {
            return false;
        }

        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i].name.empty() || fields[i].element_count() != cell_count()) {
                return false;
            }
            for (size_t j = i + 1; j < fields.size(); ++j) {
                if (fields[i].name == fields[j].name) {
                    return false;
                }
            }
        }

        return true;
    }
};

} // namespace MayaFlux::Kakshya
