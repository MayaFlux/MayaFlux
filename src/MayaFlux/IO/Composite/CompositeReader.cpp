#include "CompositeReader.hpp"

#include "MayaFlux/Kakshya/Source/CompositeContainer.hpp"

namespace MayaFlux::IO {

std::optional<Kakshya::CompositeArray>
CompositeReader::read_composite_region(const FileRegion& region)
{
    if (region.start_coordinates.size() != 1
        || region.end_coordinates.size() != 1
        || region.end_coordinates[0] < region.start_coordinates[0]
        || region.end_coordinates[0] == std::numeric_limits<uint64_t>::max()) {
        m_last_error = "Composite region requires valid one-dimensional inclusive bounds";
        return std::nullopt;
    }

    const auto start = region.start_coordinates[0];
    const auto count = region.end_coordinates[0] - start + 1;

    if (start > std::numeric_limits<size_t>::max()
        || count > std::numeric_limits<size_t>::max()
        || !seek({ start })) {
        m_last_error = "Composite region is outside the file";
        return std::nullopt;
    }

    auto data = read_next(static_cast<size_t>(count));
    if (!data || data->size() != count) {
        if (data)
            m_last_error = "Composite region extends past the end of the file";
        return std::nullopt;
    }

    return data;
}

std::shared_ptr<Kakshya::CompositeContainer>
CompositeReader::create_composite_container(size_t batch_size)
{
    auto data = read_composite();
    if (!data)
        return nullptr;

    return std::make_shared<Kakshya::CompositeContainer>(
        std::move(*data), batch_size);
}

std::vector<Kakshya::DataVariant> CompositeReader::read_all()
{
    m_last_error = "Composite data requires read_composite()";
    return {};
}

std::vector<Kakshya::DataVariant>
CompositeReader::read_region(const FileRegion&)
{
    m_last_error = "Composite data requires read_composite_region()";
    return {};
}

std::shared_ptr<Kakshya::SignalSourceContainer>
CompositeReader::create_container()
{
    m_last_error = "Composite data requires create_composite_container()";
    return nullptr;
}

bool CompositeReader::load_into_container(
    std::shared_ptr<Kakshya::SignalSourceContainer>)
{
    m_last_error = "Composite data cannot load into SignalSourceContainer";
    return false;
}

std::type_index CompositeReader::get_data_type() const
{
    return typeid(Kakshya::CompositeArray);
}

std::type_index CompositeReader::get_container_type() const
{
    return typeid(Kakshya::CompositeContainer);
}

}
