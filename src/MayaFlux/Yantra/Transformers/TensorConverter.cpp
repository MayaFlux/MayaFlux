#include "TensorConverter.hpp"

#include "MayaFlux/Kakshya/NDData/TensorConvert.hpp"

namespace MayaFlux::Yantra {

bool TensorConverter::validate_input(const input_type& input) const
{
    if (!input.data)
        return false;

    const auto view = input.data->view();
    if (!view)
        return false;

    const auto& cfg = config<TensorConversionConfig>();
    return Kakshya::diagnose_conversion(view->spec(), cfg.target, cfg.quantization).empty();
}

TensorConverter::output_type TensorConverter::operation_function(const input_type& input)
{
    if (!input.data) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "TensorConverter: the input holds no tensor container");
    }

    const auto view = input.data->view();
    if (!view) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "TensorConverter: the tensor container holds no valid tensor");
    }

    const auto& cfg = config<TensorConversionConfig>();
    auto converted = Kakshya::convert_tensor(*view, cfg.target, cfg.quantization);
    if (!converted) {
        error<std::invalid_argument>(Journal::Component::Yantra, Journal::Context::ComputeMatrix,
            std::source_location::current(), "TensorConverter: {}", converted.error());
    }

    output_type output(std::make_shared<Kakshya::TensorContainer>(std::move(*converted)));
    output.metadata = input.metadata;
    output.facets = input.facets;
    return output;
}

}
