#pragma once

#include "MayaFlux/Yantra/ComputeOperation.hpp"

namespace MayaFlux::Yantra {

/**
 * @struct TensorConversionConfig
 * @brief Settings of TensorConverter, stored whole in the operation's facets.
 */
struct TensorConversionConfig : OperationConfig {
    static constexpr auto describe_facets()
    {
        return std::tuple {
            FacetMember<&TensorConversionConfig::target> {},
            FacetMember<&TensorConversionConfig::quantization> {},
        };
    }

    /** Element type of the result. */
    Kakshya::TensorDType target { Kakshya::TensorDType::of<float>() };
    /** Quantization the result carries; the target must be an integer type of at most 32 bits. */
    std::optional<Kakshya::TensorQuantization> quantization;
};

/**
 * @class TensorConverter
 * @brief Converts the element type of a tensor, reading and writing quantization.
 *
 * Takes a tensor container and produces a new one with the target element
 * type, the source's name and axes, and dense C-order storage. A source that
 * carries quantization is dequantized; a configured quantization is applied to
 * the result. The input container is not modified. Facets and metadata of the
 * input travel with the output.
 *
 * Supported element types are whole-byte integers, bool, half, bfloat16,
 * float, double and complex of float and double. Everything else, and any
 * combination convert_tensor refuses, is reported by validate_input and
 * thrown by the operation with the reason.
 *
 * Usage:
 * @code
 * TensorConversionConfig config;
 * config.target = Kakshya::TensorDType::of<float>();
 * auto convert = std::make_shared<TensorConverter>();
 * convert->set_config(config);
 * auto result = convert->apply_operation(TensorContainerIO { weights });
 * @endcode
 */
class MAYAFLUX_API TensorConverter final
    : public ComputeOperation<std::shared_ptr<Kakshya::TensorContainer>> {
public:
    using supports = Supports<TensorConversionConfig>;

    [[nodiscard]] std::string get_name() const override { return "TensorConverter"; }

    [[nodiscard]] OperationType get_operation_type() const override { return OperationType::TRANSFORMER; }

    /** @brief The stored settings, or none when defaults are in use. */
    [[nodiscard]] ConfigRefs configs() const override
    {
        return stored_configs(supports {});
    }

    /** @brief True when the input holds a valid tensor the configured conversion accepts. */
    [[nodiscard]] bool validate_input(const input_type& input) const override;

protected:
    output_type operation_function(const input_type& input) override;
};

}
