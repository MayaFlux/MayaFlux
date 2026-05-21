#include "PlotSource.hpp"

#include "MayaFlux/Kakshya/Source/PlotContainer.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Portal::Forma::Plot {

Source::Source()
    : m_container(std::make_shared<Kakshya::PlotContainer>())
{
}

Source& Source::as(
    std::string name,
    uint64_t count,
    Kakshya::DataDimension::Role role,
    Kakshya::DataModality modality)
{
    m_last_index = m_container->add_series(std::move(name), count, role, modality);
    return *this;
}

Source& Source::from(std::shared_ptr<Nodes::Node> node)
{
    m_container->bind(m_last_index, std::move(node));
    return *this;
}

Source& Source::from(std::shared_ptr<Buffers::AudioBuffer> buf)
{
    m_container->bind(m_last_index, std::move(buf));
    return *this;
}

Source& Source::from(std::shared_ptr<Nodes::Network::NodeNetwork> net)
{
    m_container->bind(m_last_index, std::move(net));
    return *this;
}

Source& Source::from(std::function<void(std::vector<double>&)> fn)
{
    m_container->bind(m_last_index, std::move(fn));
    return *this;
}

std::shared_ptr<Kakshya::PlotContainer> Source::build()
{
    return std::move(m_container);
}

} // namespace MayaFlux::Portal::Forma::Plot
