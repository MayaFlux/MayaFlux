#pragma once

#include "Filter.hpp"

namespace MayaFlux::Nodes::Filters {

class IIR : public Filter {
protected:
    std::vector<float> coef_a;
    std::vector<float> coef_b;

    std::vector<double> x_buffer;
    std::vector<double> y_buffer;

public:
    IIR(
        std::shared_ptr<Node> input,
        const std::string& zindex_shifts,
        std::vector<float>& a_coef,
        std::vector<float>& b_coef);

protected:
    void initialize_shift_buffers() override;

    double processSample(double input) override;
};

}
