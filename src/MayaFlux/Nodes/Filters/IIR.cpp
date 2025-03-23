#include "IIR.hpp"

namespace MayaFlux::Nodes::Filters {

IIR::IIR(std::shared_ptr<Node> input, const std::string& zindex_shifts,
    std::vector<float>& a_coef, std::vector<float>& b_coef)
    : Filter(input, zindex_shifts)
    , coef_a(a_coef)
    , coef_b(b_coef)
{
    initialize_shift_buffers();
}

void IIR::initialize_shift_buffers()
{
    Filter::initialize_shift_buffers();
    x_buffer.resize(coef_a.size(), 0.f);
    y_buffer.resize(coef_b.size(), 0.f);
}

double IIR::processSample(double input)
{
    return 0.f;
}

}
