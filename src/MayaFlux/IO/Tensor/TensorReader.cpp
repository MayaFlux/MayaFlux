#include "TensorReader.hpp"

namespace MayaFlux::IO {

std::optional<Kakshya::TensorSet> TensorReader::read_tensors(const TensorReadOptions& options)
{
    if (!is_open()) {
        fail("no file is open");
        return std::nullopt;
    }

    std::vector<std::string> names = options.names;
    if (names.empty()) {
        for (const auto& spec : describe())
            names.push_back(spec.name);
    }

    Kakshya::TensorSet set;
    set.metadata = tensor_metadata();
    set.tensors.reserve(names.size());
    for (const auto& name : names) {
        auto tensor = read(name);
        if (!tensor)
            return std::nullopt;
        set.tensors.push_back(std::move(*tensor));
    }
    return set;
}

std::optional<Kakshya::TensorSpec> TensorReader::find_spec(std::string_view name) const
{
    for (auto& spec : describe()) {
        if (spec.name == name)
            return std::move(spec);
    }
    return std::nullopt;
}

std::optional<FileMetadata> TensorReader::get_metadata() const
{
    if (!is_open())
        return std::nullopt;

    FileMetadata metadata;
    metadata.format = format_name();
    std::error_code error;
    const auto size = std::filesystem::file_size(m_path, error);
    metadata.file_size = error ? 0 : size;
    return metadata;
}

std::vector<Kakshya::DataVariant> TensorReader::read_all()
{
    m_last_error = "Tensor data requires read() or read_tensors()";
    return {};
}

std::vector<Kakshya::DataVariant> TensorReader::read_region(const FileRegion&)
{
    m_last_error = "Tensor data requires read() or read_tensors()";
    return {};
}

std::shared_ptr<Kakshya::SignalSourceContainer> TensorReader::create_container()
{
    m_last_error = "Tensor data does not use a SignalSourceContainer";
    return nullptr;
}

bool TensorReader::load_into_container(std::shared_ptr<Kakshya::SignalSourceContainer>)
{
    return fail("Tensor data cannot load into a SignalSourceContainer");
}

std::type_index TensorReader::get_data_type() const
{
    return typeid(Kakshya::TensorSet);
}

std::type_index TensorReader::get_container_type() const
{
    return typeid(Kakshya::TensorSet);
}

}
