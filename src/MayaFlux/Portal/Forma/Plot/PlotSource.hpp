#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Nodes {
class Node;
namespace Network {
    class NodeNetwork;
}
}

namespace MayaFlux::Buffers {
class AudioBuffer;
}

namespace MayaFlux::Kakshya {
class PlotContainer;
}

namespace MayaFlux::Portal::Forma::Plot {

/**
 * @class PlotSource
 * @brief Chainable builder for PlotContainer construction.
 *
 * Eliminates manual container creation, index threading, and
 * mark_ready_for_processing() calls. .as() adds a series and records its
 * index; the following .with() binds a source to it. Pairs may be chained
 * indefinitely. Call .build() as the terminal to obtain the container.
 *
 * @code
 * auto container = PlotSource()
 *     .as("fm_sine", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D).with(sine)
 *     .as("mod",     512, Role::SPATIAL_Y, DataModality::AUDIO_1D).with(mod)
 *     .build();
 * @endcode
 */
class MAYAFLUX_API PlotSource {
public:
    PlotSource();

    /**
     * @brief Add a named series and record its index for the next with() call.
     * @param name     Series name.
     * @param count    Sample count.
     * @param role     Semantic role.
     * @param modality Data modality.
     */
    PlotSource& as(
        std::string name,
        uint64_t count,
        Kakshya::DataDimension::Role role,
        Kakshya::DataModality modality);

    /** @brief Bind a Node to the last series added by as(). */
    PlotSource& from(std::shared_ptr<Nodes::Node> node);

    /** @brief Bind an AudioBuffer to the last series added by as(). */
    PlotSource& from(std::shared_ptr<Buffers::AudioBuffer> buf);

    /** @brief Bind a NodeNetwork to the last series added by as(). */
    PlotSource& from(std::shared_ptr<Nodes::Network::NodeNetwork> net);

    /** @brief Bind a callable to the last series added by as(). */
    PlotSource& from(std::function<void(std::vector<double>&)> fn);

    /**
     * @brief Finalise and return the constructed container.
     * @return Ready PlotContainer with all series added and sources bound.
     */
    [[nodiscard]] std::shared_ptr<Kakshya::PlotContainer> build();

private:
    std::shared_ptr<Kakshya::PlotContainer> m_container;
    uint32_t m_last_index { 0 };
};

} // namespace MayaFlux::Portal::Forma::Plot
