#pragma once

#include "MayaFlux/Yantra/ComputeOperation.hpp"

namespace MayaFlux::Yantra {

/**
 * @class FunctionalOperation
 * @brief Wraps a single callable as a ComputeOperation, for a wild or
 *        one-off idea that does not warrant a hand-written class.
 *
 * Satisfies ComputeOperation's pure virtuals with the smallest
 * possible surface: operation_function delegates to the supplied
 * function; get_operation_type always returns OperationType::CUSTOM, since a
 * callable supplied at construction time carries no inherent verb
 * category. Fully interchangeable with any hand-written
 * ComputeOperation<InputType, OutputType> subclass everywhere
 * ComputeMatrix and dependency graphs are
 * concerned: none of that machinery keys off get_operation_type().
 *
 * @tparam InputType  ComputeData type accepted.
 * @tparam OutputType ComputeData type produced. Defaults to InputType.
 */
template <ComputeData InputType = std::vector<Kakshya::DataVariant>,
    ComputeData OutputType = InputType>
class FunctionalOperation : public ComputeOperation<InputType, OutputType> {
public:
    using input_type = Datum<InputType>;
    using output_type = Datum<OutputType>;
    using Fn = std::function<output_type(const input_type&)>;

    explicit FunctionalOperation(Fn fn)
        : m_fn(std::move(fn))
    {
    }

    [[nodiscard]] OperationType get_operation_type() const override
    {
        return OperationType::CUSTOM;
    }

protected:
    output_type operation_function(const input_type& input) override
    {
        return m_fn(input);
    }

private:
    Fn m_fn;
};

} // namespace MayaFlux::Yantra
