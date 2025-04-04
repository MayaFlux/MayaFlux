#pragma once

#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

class IIR : public Filter {
public:
    IIR(std::shared_ptr<Node> input, const std::string& zindex_shifts);
    IIR(std::shared_ptr<Node> input, std::vector<double> a_coef, std::vector<double> b_coef);

protected:
    double process_sample(double input) override;
};

}
