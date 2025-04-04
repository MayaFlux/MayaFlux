#pragma once

#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

class FIR : public Filter {
public:
    FIR(std::shared_ptr<Node> input, const std::string& zindex_shifts);
    FIR(std::shared_ptr<Node> input, const std::vector<double> coeffs);

protected:
    double process_sample(double input) override;
};

}
